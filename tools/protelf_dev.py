#!/usr/bin/env python3
import argparse
import base64
import binascii
import hashlib
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path

# Importable both as "tools.protelf_dev" (test suite) and as a bare script.
try:
    from . import protelf_resources
    from .protelf_sdk import add_sdk_profile_arg, sdk_sources, sdk_libraries
except ImportError:  # pragma: no cover - direct `python tools/protelf_dev.py`
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import protelf_resources
    from protelf_sdk import add_sdk_profile_arg, sdk_sources, sdk_libraries


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CHUNK = 96
# Absolute tool-side ELF ceiling: the widest flash-XIP run the arena can
# express. This is a backstop, not the real limit -- the exact bound is the
# span an individual image declares, and it is enforced twice with that
# knowledge in hand: by warn_loaded_layout() here and by
# protelf_manifest_elf_ceiling() in the kernel. Generic validators that see an
# ELF without its manifest can only check the backstop.
# Whole-bundle ceiling. Deliberately independent of MAX_ELF_BYTES: resources
# may be megabytes, but the executable still has to fit its flash-XIP run, so
# raising this must never raise the ELF limit.
MAX_BUNDLE_BYTES = 4 * 1024 * 1024
PROTOELF_MIN_HEAP_BYTES = 4096
# Optimization levels a ProtoELF may be built at. -Os stays the default for
# every app: these images live in a 128 KiB flash-XIP slot and share the L2
# cache with the kernel, so size is the right default trade. An app opts into
# something else per-app, with a measurement, via `optimize=` metadata or
# --opt -- never by changing the default here.
PROTOELF_OPT_LEVELS = {"Os", "O0", "O1", "O2", "O3"}
PROTOELF_DEFAULT_OPT = "Os"
# The XIP slot geometry, mirrored from main/kernel/loader/protelf_xip.h. An ABI 5
# image gets an aligned run of 128 KiB flash slots for RX and matching PSRAM
# slots for RW, paired at a fixed virtual bias.
PROTELF_XIP_SLOT_BYTES = 128 * 1024
# Widest naturally aligned run the 4 MiB arena can express; see
# main/kernel/loader/protelf_xip.h. An image declares its span with
# --slot-span-log2 and the kernel refuses anything wider.
PROTELF_XIP_SLOT_COUNT = 31
PROTELF_XIP_SPAN_LOG2_MAX = PROTELF_XIP_SLOT_COUNT.bit_length() - 1
PROTELF_XIP_SPAN_SLOTS_MAX = 1 << PROTELF_XIP_SPAN_LOG2_MAX
MAX_ELF_BYTES = PROTELF_XIP_SLOT_BYTES * PROTELF_XIP_SPAN_SLOTS_MAX
# The kernel's one boot-reserved large-heap pool (PROTO_ELF_LARGE_HEAP_LOG2 in
# main/include/proto_abi.h). A manifest may ask for it with
# large-heap-log2=16..23; the quota ceiling grows by the requested slice size.
PROTO_ELF_LARGE_HEAP_LOG2_MIN = 16
PROTO_ELF_LARGE_HEAP_LOG2 = 23
PROTO_ELF_LARGE_HEAP_BYTES = 1 << PROTO_ELF_LARGE_HEAP_LOG2


def xip_span_bytes(span_log2: int) -> int:
    if not 0 <= span_log2 <= PROTELF_XIP_SPAN_LOG2_MAX:
        raise SystemExit(
            f"slot-span-log2 must be 0..{PROTELF_XIP_SPAN_LOG2_MAX}"
        )
    return PROTELF_XIP_SLOT_BYTES << span_log2
PROTELF_XIP_FLASH_PSRAM_DELTA = 0x08000000
PROTOELF_STATIC_WINDOW16_BYTES = 16 * 1024
PROTOELF_DEFAULT_QUOTA_BYTES = 16 * 1024
EM_RISCV = 243
ET_DYN = 3
PT_INTERP = 3
PT_DYNAMIC = 2
PT_LOAD = 1
PT_TLS = 7
PT_GNU_STACK = 0x6474E551
PF_X = 1
PF_W = 2
PF_R = 4
DT_NULL = 0
DT_NEEDED = 1
DT_PLTRELSZ = 2
DT_RELA = 7
DT_RELASZ = 8
DT_JMPREL = 23
DT_REL = 17
DT_TEXTREL = 22
DT_SONAME = 14
DT_STRTAB = 5
DT_STRSZ = 10
DT_RELAENT = 9
DT_PLTREL = 20
DT_FLAGS = 30
DT_VERSYM = 0x6FFFFFF0
DT_VERDEF = 0x6FFFFFFC
DT_VERNEED = 0x6FFFFFFE
DF_TEXTREL = 0x4
R_RISCV_32 = 1
R_RISCV_RELATIVE = 3
R_RISCV_COPY = 4
R_RISCV_JUMP_SLOT = 5
PROTO_INTERP = b"/sys/protold.so\0"
PROTO_OBJECT_MAGIC = 0x504F424A
PROTO_OBJECT_VERSION = 1
PROTO_OBJECT_HEADER_SIZE = 32
PROTO_OBJECT_ENTRY_SIZE = 152
PROTO_OBJECT_REQUIRED = 1 << 1
PROTO_OBJECT_OPTIONAL = 1 << 2
PROTO_OBJECT_GLOBAL = 1 << 5
MAX_OBJECTS = 8
PROTO_ELF_ABI_VERSION = 5
PROTO_ELF_ABI_VERSION_V4 = 4
PROTO_ELF_ABI_VERSIONS_SUPPORTED = {
    PROTO_ELF_ABI_VERSION_V4,
    PROTO_ELF_ABI_VERSION,
}
PROTO_ELF_MANIFEST_MAGIC = 0x50454C46
PROTO_ELF_MANIFEST_VERSION_V3 = 3
PROTO_ELF_MANIFEST_VERSION_V4 = 4
PROTO_ELF_MANIFEST_VERSION = 5
PROTO_ELF_MANIFEST_VERSIONS_SUPPORTED = {
    PROTO_ELF_MANIFEST_VERSION_V3,
    PROTO_ELF_MANIFEST_VERSION_V4,
    PROTO_ELF_MANIFEST_VERSION,
}
PROTO_ELF_MANIFEST_SIZE = 420
PROTO_ELF_LAYOUT_SPLIT_DYNAMIC = 1 << 0
PROTO_ELF_LAYOUT_FLASH_XIP = 1 << 1
PROTO_ELF_LAYOUT_FLAGS_SUPPORTED = (
    PROTO_ELF_LAYOUT_SPLIT_DYNAMIC | PROTO_ELF_LAYOUT_FLASH_XIP
)
# Manifest offsets claimed by ABI 5 for the resource section. reserved[32]
# becomes sha256(resource_table); the first 16 bytes of reserved_v2[24] become
# <4I>(resource_count, table_size, payload_size, 0).
MANIFEST_OFF_RESOURCE_HASH = 228
MANIFEST_OFF_RESOURCE_META = 332
PROTO_ELF_SIGNATURE_ECDSA_P256_SHA256 = 1

KERNEL_CAP_MEM_ALLOC = 1 << 2
KERNEL_CAP_MEM_FREE = 1 << 3
KERNEL_CAP_IPC_RECV = 1 << 5
KERNEL_CAP_PROCESS_EXIT = 1 << 7
KERNEL_CAP_SLEEP = 1 << 8
# bit 15 (legacy KERNEL_CAP_STORAGE) retired; use scoped storage caps below.
KERNEL_CAP_SHARED_MEMORY = 1 << 18
KERNEL_CAP_THREAD_CREATE = 1 << 27
KERNEL_CAP_RESOURCE = 1 << 29
# Game3D asset loader entry points (main/include/proto_3d.h). The loader is
# linked SDK code, so its resource reads and preload worker are invisible in
# app text; test_protelf_metadata checks this list against the header.
G3A_LOAD_CALLS = ("proto_3d_asset_load", "proto_3d_asset_load_ex")
G3A_PRELOAD_CALLS = ("proto_3d_asset_preload", "proto_3d_asset_preload_ex", "proto_3d_asset_preload_opt")

PERM_STORAGE_PRIVATE = 0x1
PERM_IPC_LOGGER = 0x2
PERM_UI = 0x4
PERM_FILE_DIALOG = 0x10
PERM_STORAGE_SHARED = 0x8
PERM_WIFI = 0x20
PERM_AUDIO = 0x40
# Inbound listen. Unlike every other permission here, declaring it in an
# unsigned dev bundle does NOT grant it: KERNEL_CAP_NETWORK_LISTEN is
# outside KERNEL_CAP_UNSIGNED_ALLOWED, so the kernel withholds it at
# install and the app installs PENDING_PERMISSIONS. Sign the bundle.
PERM_NETWORK_LISTEN = 0x80
PERM_SIMD = 0x100
# ESP32-P4 base ISA for ProtoELF (RV32F only, no RV32D). build_app() appends
# "_xesppie" for apps holding PERM_SIMD.
PROTOELF_MARCH = "rv32imafc_zicsr_zifencei"
LOGGER_PID = 3
IPC_TARGET_LOGGER = 1 << LOGGER_PID

APP_FLAG_HIDDEN = 1 << 3
APP_FLAG_HEAVY_INPUT = 1 << 4
APP_FLAG_EXIT_HOOK = 1 << 9

AUTO_SOURCE_SUFFIXES = {".c", ".h", ".cc", ".cpp", ".hpp", ".s"}


def align_up(value: int, alignment: int) -> int:
    if alignment <= 0 or alignment & (alignment - 1):
        raise SystemExit(f"invalid alignment {alignment}")
    return (value + alignment - 1) & ~(alignment - 1)


def round_power2(requested: int) -> int:
    if requested <= 0:
        raise SystemExit("invalid zero-sized window request")
    rounded = 64
    while rounded < requested:
        rounded <<= 1
    return rounded


def parse_auto_int(text: str) -> int | None:
    if text.strip().lower() == "auto":
        return None
    return int(text, 0)


def import_serial():
    try:
        import serial
    except ImportError as exc:
        raise SystemExit("pyserial is required: python -m pip install pyserial") from exc
    return serial


def rel(path: Path) -> Path:
    return path if path.is_absolute() else REPO_ROOT / path


def fixed_app_stem(name: str) -> str:
    return "".join(ch if ch.isalnum() or ch in ("-", "_") else "-" for ch in name)


def package_stem(name: str) -> str:
    out = []
    prev_dot = False
    for ch in name.lower():
        if ch.isalnum() or ch == "-":
            out.append(ch)
            prev_dot = False
        elif ch in ("_", "."):
            if out and not prev_dot:
                out.append(".")
                prev_dot = True
        elif out and not prev_dot:
            out.append(".")
            prev_dot = True
    stem = "".join(out).strip(".")
    return stem or "app"


def sha256_hex(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run_cmd(cmd, *, cwd=REPO_ROOT):
    print("+ " + " ".join(str(part) for part in cmd))
    subprocess.run([str(part) for part in cmd], cwd=cwd, check=True)


def find_gcc(name: str) -> str:
    found = shutil.which(name)
    if found:
        return found
    if os.name == "nt" and name == "riscv32-esp-elf-gcc":
        tools = Path("C:/Espressif/tools/riscv32-esp-elf")
        if tools.exists():
            preferred = tools / "esp-14.2.0_20260121" / "riscv32-esp-elf" / "bin" / "riscv32-esp-elf-gcc.exe"
            if preferred.exists():
                return str(preferred)
            candidates = sorted(tools.glob("*/riscv32-esp-elf/bin/riscv32-esp-elf-gcc.exe"))
            if candidates:
                return str(candidates[-1])
    raise SystemExit(
        f"{name} not found. Run ESP-IDF export.ps1 first, or pass --gcc with the full compiler path."
    )


def validate_elf(path: Path, *, dynamic_ok: bool = False, library: bool = False):
    data = path.read_bytes()
    if len(data) == 0 or len(data) > MAX_ELF_BYTES:
        raise SystemExit(f"ELF size must be 1..{MAX_ELF_BYTES} bytes")
    if len(data) < 52 or data[:4] != b"\x7fELF":
        raise SystemExit("not an ELF file")
    if data[4] != 1 or data[5] != 1:
        raise SystemExit("ProtoELF v1 requires ELFCLASS32 little-endian")

    (
        e_type,
        e_machine,
        _e_version,
        _e_entry,
        e_phoff,
        _e_shoff,
        _e_flags,
        e_ehsize,
        e_phentsize,
        e_phnum,
        _e_shentsize,
        _e_shnum,
        _e_shstrndx,
    ) = struct.unpack_from("<HHIIIIIHHHHHH", data, 16)
    if e_type != ET_DYN or e_machine != EM_RISCV:
        raise SystemExit("ProtoELF v1 requires RISC-V ET_DYN")
    if e_ehsize != 52 or e_phentsize != 32:
        raise SystemExit("unexpected ELF/program-header size")
    if e_phoff + e_phnum * e_phentsize > len(data):
        raise SystemExit("program headers exceed ELF size")

    dynamic_ranges = []
    dynamic_tags = []
    interp = None
    load_segments = []
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_type, p_offset, _p_vaddr, _p_paddr, p_filesz, _p_memsz, _p_flags, _p_align = struct.unpack_from(
            "<IIIIIIII", data, off
        )
        if p_offset + p_filesz > len(data):
            raise SystemExit("segment exceeds ELF size")
        if p_type == PT_LOAD:
            if p_filesz > _p_memsz or _p_memsz == 0:
                raise SystemExit("invalid PT_LOAD file/memory size")
            if (_p_flags & (PF_W | PF_X)) == (PF_W | PF_X):
                raise SystemExit("RWX PT_LOAD segments are not allowed")
            load_segments.append(
                (_p_vaddr, _p_memsz, p_filesz, _p_flags, _p_align, p_offset)
            )
        if p_type == PT_TLS:
            raise SystemExit("PT_TLS is not supported by the current loaded-app ABI")
        if p_type == PT_GNU_STACK and (_p_flags & PF_X):
            raise SystemExit("executable stacks are not allowed")
        if p_type == PT_INTERP:
            interp = data[p_offset : p_offset + p_filesz]
        if p_type == PT_DYNAMIC:
            dynamic_ranges.append((p_offset, p_filesz))

    for p_offset, p_filesz in dynamic_ranges:
        if p_filesz % 8 != 0:
            raise SystemExit("dynamic table has invalid size")
        for off in range(p_offset, p_offset + p_filesz, 8):
            tag, val = struct.unpack_from("<iI", data, off)
            if tag == DT_NULL:
                break
            dynamic_tags.append((tag, val))
            if tag in (DT_REL, DT_TEXTREL):
                raise SystemExit(f"unsupported dynamic tag {tag} value=0x{val:x}")
            if tag in (DT_VERSYM, DT_VERDEF, DT_VERNEED):
                raise SystemExit("GNU symbol versions are not supported yet")
            if tag == DT_FLAGS and (val & DF_TEXTREL):
                raise SystemExit("text relocations are not allowed")
            if not dynamic_ok and tag in (DT_NEEDED, DT_JMPREL):
                raise SystemExit(f"unsupported dynamic tag {tag} value=0x{val:x}")
    if library and interp is not None:
        raise SystemExit("shared libraries must not contain PT_INTERP")
    if not library and dynamic_ok and interp != PROTO_INTERP:
        raise SystemExit("dynamic ProtoELF apps require PT_INTERP /sys/protold.so")
    if not dynamic_ok and interp is not None:
        raise SystemExit("PT_INTERP is not allowed for a standalone ProtoELF app")
    if dynamic_ok:
        rx_loads = 0
        rw_loads = 0
        for seg_index, (vaddr, memsz, _filesz, flags, align,
                        offset) in enumerate(load_segments):
            end = vaddr + memsz
            if end > 0xFFFFFFFF or align > 256 or (
                align > 1 and (align & (align - 1)) != 0
            ) or (
                align > 1 and (vaddr & (align - 1)) != (offset & (align - 1))
            ):
                raise SystemExit("ProtoELF split-dynamic segment alignment is invalid")
            for other_index, (other_vaddr, other_size, _other_filesz,
                              _other_flags, _other_align,
                              _other_offset) in enumerate(load_segments):
                if other_index >= seg_index:
                    break
                if vaddr < other_vaddr + other_size and other_vaddr < end:
                    raise SystemExit(
                        f"overlapping PT_LOAD segments {other_index} and {seg_index}"
                    )
            if flags & PF_W:
                rw_loads += 1
            else:
                rx_loads += 1
        if rx_loads != 1 or rw_loads != 1:
            raise SystemExit("ProtoELF split-dynamic requires one RX and one RW PT_LOAD")

        def file_blob_at_vaddr(vaddr: int, size: int) -> bytes:
            for start, _memsz, filesz, _flags, _align, file_offset in load_segments:
                if start <= vaddr and size <= start + filesz - vaddr:
                    offset = file_offset + vaddr - start
                    if offset + size <= len(data):
                        return data[offset : offset + size]
            raise SystemExit(
                f"dynamic address 0x{vaddr:x}+{size} is outside file-backed PT_LOAD data"
            )

        def tag_value(tag: int, default=None):
            return next((value for key, value in dynamic_tags if key == tag), default)

        rela_ent = tag_value(DT_RELAENT, 12)
        if rela_ent != 12:
            raise SystemExit("RISC-V RELA entries must be 12 bytes")
        if tag_value(DT_PLTRELSZ, 0) and tag_value(DT_PLTREL) != DT_RELA:
            raise SystemExit("PLT relocations must use RELA")
        relocation_tables = (
            (tag_value(DT_RELA), tag_value(DT_RELASZ, 0)),
            (tag_value(DT_JMPREL), tag_value(DT_PLTRELSZ, 0)),
        )
        seen_tables = set()
        for address, size in relocation_tables:
            if address is None or size == 0:
                continue
            if any(
                prior_address <= address and
                address + size <= prior_address + prior_size
                for prior_address, prior_size in seen_tables
            ):
                continue
            if any(
                address < prior_address + prior_size and
                prior_address < address + size
                for prior_address, prior_size in seen_tables
            ):
                raise SystemExit("relocation tables overlap inconsistently")
            seen_tables.add((address, size))
            if size % 12:
                raise SystemExit("relocation table size is not a multiple of RELA")
            blob = file_blob_at_vaddr(address, size)
            for off in range(0, size, 12):
                target, info, _addend = struct.unpack_from("<IIi", blob, off)
                kind = info & 0xFF
                symbol = info >> 8
                if kind not in (R_RISCV_32, R_RISCV_RELATIVE,
                                R_RISCV_JUMP_SLOT):
                    label = "R_RISCV_COPY" if kind == R_RISCV_COPY else str(kind)
                    raise SystemExit(f"unsupported relocation type {label}")
                if kind == R_RISCV_RELATIVE and symbol != 0:
                    raise SystemExit("R_RISCV_RELATIVE must not reference a symbol")
                writable_target = any(
                    (flags & PF_W) and start <= target and
                    target + 4 <= start + memsz
                    for start, memsz, _filesz, flags, _align,
                    _offset in load_segments
                )
                if not writable_target:
                    raise SystemExit(
                        f"relocation target 0x{target:x} is outside writable PT_LOAD data"
                    )


def elf_dynamic_info(path: Path, *, library: bool = False) -> dict:
    data = path.read_bytes()
    validate_elf(path, dynamic_ok=True, library=library)
    (
        _e_type,
        _e_machine,
        _e_version,
        _e_entry,
        e_phoff,
        _e_shoff,
        _e_flags,
        _e_ehsize,
        e_phentsize,
        e_phnum,
        _e_shentsize,
        _e_shnum,
        _e_shstrndx,
    ) = struct.unpack_from("<HHIIIIIHHHHHH", data, 16)
    loads = []
    dynamic = None
    for i in range(e_phnum):
        fields = struct.unpack_from("<IIIIIIII", data, e_phoff + i * e_phentsize)
        p_type, p_offset, p_vaddr, _p_paddr, p_filesz, _p_memsz, _p_flags, _p_align = fields
        if p_type == PT_LOAD:
            loads.append(
                {
                    "vaddr": p_vaddr,
                    "file_end": p_vaddr + p_filesz,
                    "mem_end": p_vaddr + _p_memsz,
                    "offset": p_offset,
                    "flags": _p_flags,
                    "align": _p_align,
                }
            )
        elif p_type == PT_DYNAMIC:
            dynamic = (p_offset, p_filesz)
    if dynamic is None:
        raise SystemExit(f"{path} has no PT_DYNAMIC")

    def vaddr_blob(vaddr: int, size: int) -> bytes:
        for load in loads:
            start = load["vaddr"]
            end = load["file_end"]
            if start <= vaddr <= end and size <= end - vaddr:
                file_off = load["offset"] + vaddr - start
                return data[file_off : file_off + size]
        raise SystemExit(f"{path}: dynamic address 0x{vaddr:x} is not file-backed")

    tags = []
    for off in range(dynamic[0], dynamic[0] + dynamic[1], 8):
        tag, value = struct.unpack_from("<iI", data, off)
        if tag == DT_NULL:
            break
        tags.append((tag, value))
    strtab = next((value for tag, value in tags if tag == DT_STRTAB), None)
    strsz = next((value for tag, value in tags if tag == DT_STRSZ), None)
    strings = vaddr_blob(strtab, strsz) if strtab is not None and strsz else b""

    def dyn_string(offset: int) -> str:
        if offset >= len(strings):
            raise SystemExit(f"{path}: dynamic string offset is out of bounds")
        end = strings.find(b"\0", offset)
        if end < 0:
            raise SystemExit(f"{path}: unterminated dynamic string")
        return strings[offset:end].decode("ascii")

    soname_offset = next((value for tag, value in tags if tag == DT_SONAME), None)
    needed = [dyn_string(value) for tag, value in tags if tag == DT_NEEDED]
    relocation_tables = []
    rela = next((value for tag, value in tags if tag == DT_RELA), None)
    rela_size = next((value for tag, value in tags if tag == DT_RELASZ), 0)
    jmprel = next((value for tag, value in tags if tag == DT_JMPREL), None)
    jmprel_size = next((value for tag, value in tags if tag == DT_PLTRELSZ), 0)
    for address, size in ((rela, rela_size), (jmprel, jmprel_size)):
        if address is None or size == 0:
            continue
        if any(
            prior_address <= address and
            address + size <= prior_address + prior_size
            for prior_address, prior_size in relocation_tables
        ):
            continue
        if any(
            address < prior_address + prior_size and
            prior_address < address + size
            for prior_address, prior_size in relocation_tables
        ):
            raise SystemExit(f"{path}: relocation tables overlap inconsistently")
        relocation_tables.append((address, size))
    relocations = []
    for address, size in relocation_tables:
        blob = vaddr_blob(address, size)
        for off in range(0, size, 12):
            target, info, addend = struct.unpack_from("<IIi", blob, off)
            relocations.append((target, info & 0xFF, info >> 8, addend))
    return {
        "soname": dyn_string(soname_offset) if soname_offset is not None else None,
        "needed": needed,
        "tags": tags,
        "loads": loads,
        "relocations": relocations,
    }


def protelf_layout_info(path: Path, heap_bytes: int,
                        abi: int = PROTO_ELF_ABI_VERSION,
                        span_log2: int = 0) -> dict | None:
    info = elf_dynamic_info(path)
    rx_loads = [load for load in info["loads"] if load["flags"] & PF_X]
    rw_loads = [load for load in info["loads"] if load["flags"] & PF_W]
    if len(rx_loads) != 1 or len(rw_loads) != 1:
        return None

    rx = rx_loads[0]
    rw = rw_loads[0]
    alignment = max(16, rx["align"], rw["align"])
    rx_span = align_up(rx["mem_end"] - rx["vaddr"], alignment)
    rw_reserved = align_up(rw["mem_end"] - rw["vaddr"], alignment)
    requested_heap = max(heap_bytes, PROTOELF_MIN_HEAP_BYTES)
    rx_window = rw["vaddr"] - rx["vaddr"]
    if abi == PROTO_ELF_ABI_VERSION:
        # rx_window is the linked flash->PSRAM bias, not an allocation size.
        # Both windows are the width of the declared span, which the arena
        # pairs at the same offset; see proteld_assign_layout().
        rw_window = xip_span_bytes(span_log2)
        budget = rw_window
    else:
        rw_window = round_power2(rw_reserved + requested_heap)
        if rw_window < rx_window:
            rw_window = rx_window
        budget = PROTOELF_STATIC_WINDOW16_BYTES
    return {
        "rx_span": rx_span,
        "rw_reserved": rw_reserved,
        "requested_heap": requested_heap,
        "rx_window": rx_window,
        "rw_window": rw_window,
        "budget": budget,
        "max_heap": max(0, budget - rw_reserved),
    }


def warn_loaded_layout(path: Path, heap_bytes: int, app_name: str,
                       abi: int = PROTO_ELF_ABI_VERSION,
                       span_log2: int = 0):
    layout = protelf_layout_info(path, heap_bytes, abi, span_log2)
    if layout is None:
        return
    needed = layout["rw_reserved"] + layout["requested_heap"]
    if abi == PROTO_ELF_ABI_VERSION:
        # Hard ceiling, not advice: the span is a fixed partition geometry.
        span = xip_span_bytes(span_log2)
        slots = 1 << span_log2
        if layout["rx_span"] > span:
            raise SystemExit(
                f"{app_name}: RX span {layout['rx_span']} bytes exceeds the "
                f"{span}-byte flash-XIP run ({slots} slot(s)); raise "
                f"--slot-span-log2, split the app, or move bulk data into "
                f"--resource sections."
            )
        if needed > span:
            raise SystemExit(
                f"{app_name}: RW reserved {layout['rw_reserved']} + heap "
                f"{layout['requested_heap']} exceeds the "
                f"{span}-byte PSRAM run ({slots} slot(s)); raise "
                f"--slot-span-log2 or use --heap <= {layout['max_heap']}."
            )
        return
    if layout["rw_window"] > PROTOELF_STATIC_WINDOW16_BYTES:
        print(
            "[protelf warning] "
            f"{app_name}: RW reserved={layout['rw_reserved']} bytes, "
            f"heap={layout['requested_heap']} bytes -> RW window="
            f"{layout['rw_window']} bytes. ABI 4 needs that much CONTIGUOUS "
            f"aligned internal DRAM instead of the guaranteed 16 KiB static "
            f"window; use --heap <= {layout['max_heap']} or rebuild with "
            f"--abi {PROTO_ELF_ABI_VERSION}."
        )
        if "game2d" in app_name.lower() and layout["requested_heap"] > PROTOELF_MIN_HEAP_BYTES:
            print(
                "[protelf warning] "
                "Game2D sprites are copied into kernel-owned assets after upload; "
                "prefer --heap 4096 unless your app keeps large mutable state."
            )


def auto_loaded_quota(path: Path, heap_bytes: int,
                      abi: int = PROTO_ELF_ABI_VERSION,
                      span_log2: int = 0) -> int:
    layout = protelf_layout_info(path, heap_bytes, abi, span_log2)
    if layout is None:
        return PROTOELF_DEFAULT_QUOTA_BYTES
    usable = max(0, layout["rw_window"] - layout["rw_reserved"])
    if usable <= 0:
        return PROTOELF_MIN_HEAP_BYTES
    return max(PROTOELF_MIN_HEAP_BYTES, usable)


def app_source_files(app_dir: Path) -> list[Path]:
    app_dir = rel(app_dir)
    if not app_dir.exists():
        return []
    return [
        path for path in sorted(app_dir.rglob("*"))
        if path.is_file()
        and path.suffix.lower() in AUTO_SOURCE_SUFFIXES
        and "loaded_apps/sdk" not in path.as_posix().replace("\\", "/")
    ]


def _has_call(text: str, name: str) -> bool:
    return re.search(rf"\b{name}\s*\(", text) is not None


def _parse_auto_int(value: str, label: str) -> int:
    try:
        return int(value.strip(), 0)
    except ValueError as exc:
        raise SystemExit(f"invalid proto metadata {label}={value!r}") from exc


def detect_app_metadata(app_dir: Path) -> dict:
    """Infer manifest bits from app source plus optional // proto: metadata."""
    files = app_source_files(app_dir)
    if not files:
        return {}
    chunks = []
    for path in files:
        try:
            chunks.append(path.read_text(encoding="utf-8", errors="ignore"))
        except OSError:
            continue
    text = "\n".join(chunks)
    caps = 0
    required_permissions = 0
    optional_permissions = 0
    app_flags = 0
    scalar_overrides: dict[str, int] = {}
    string_overrides: dict[str, str] = {}
    reasons: list[str] = []

    call_caps = {
        "proto_malloc": (KERNEL_CAP_MEM_ALLOC, "malloc"),
        "proto_free": (KERNEL_CAP_MEM_FREE, "free"),
        "proto_recv": (KERNEL_CAP_IPC_RECV, "ipc-recv"),
        "proto_exit": (KERNEL_CAP_PROCESS_EXIT, "exit"),
        "proto_sleep_ms": (KERNEL_CAP_SLEEP, "sleep"),
        "proto_thread_yield": (KERNEL_CAP_SLEEP, "sleep"),
        "proto_thread_create": (KERNEL_CAP_THREAD_CREATE, "thread-create"),
        "proto_shm_open": (KERNEL_CAP_SHARED_MEMORY, "shared-memory"),
        "proto_shm_map": (KERNEL_CAP_SHARED_MEMORY, "shared-memory"),
        "proto_resource_open": (KERNEL_CAP_RESOURCE, "resource"),
        "proto_resource_read": (KERNEL_CAP_RESOURCE, "resource"),
    }
    for call, (bit, reason) in call_caps.items():
        if _has_call(text, call):
            caps |= bit
            reasons.append(reason)
    # The Game3D asset loader is linked SDK code (proto_3d_asset.c): its
    # resource reads and its preload worker are invisible in app text.
    loads = G3A_LOAD_CALLS
    preloads = G3A_PRELOAD_CALLS
    if any(_has_call(text, call) for call in loads + preloads):
        caps |= KERNEL_CAP_RESOURCE
        reasons.append("g3a-asset")
    if any(_has_call(text, call) for call in preloads):
        caps |= KERNEL_CAP_THREAD_CREATE
        reasons.append("g3a-preload")

    # proto_gfx is linked SDK code rather than text in the app directory, so
    # source inference must account for the ABI calls hidden behind init/frame
    # lifecycle. This keeps a shapes-only retained app from receiving UI but
    # missing the allocator or pacing capabilities used by the library.
    uses_proto_gfx = _has_call(text, "proto_gfx_init")
    uses_proto_3d = _has_call(text, "proto_3d_init") or _has_call(text, "proto_3d_query_caps")
    if uses_proto_gfx or uses_proto_3d:
        caps |= KERNEL_CAP_MEM_ALLOC | KERNEL_CAP_MEM_FREE | KERNEL_CAP_SLEEP
        reasons.extend(("malloc", "free", "sleep"))

    uses_file_dialog = (_has_call(text, "proto_file_dialog_open") or
                        _has_call(text, "proto_file_dialog_save"))
    storage_calls = [
        "proto_open", "proto_stat", "proto_mkdir", "proto_unlink", "proto_listdir",
    ]
    if not uses_file_dialog:
        storage_calls.extend(("proto_close", "proto_read", "proto_write",
                              "proto_read_buf", "proto_write_buf",
                              "proto_file_rewind_truncate"))
    if any(_has_call(text, call) for call in storage_calls):
        reasons.append("storage")
        if '"/shared/' in text:
            required_permissions |= PERM_STORAGE_SHARED
        if '"/self/' in text or not (required_permissions & PERM_STORAGE_SHARED):
            required_permissions |= PERM_STORAGE_PRIVATE
    if uses_file_dialog:
        required_permissions |= PERM_FILE_DIALOG | PERM_UI

    ui_calls = (
        "proto_ui_open_surface", "proto_ui_submit_commands",
        "proto_ui_scroll", "proto_ui_close_surface",
        "proto_ui_background_request", "proto_ui_exit_request",
    )
    if any(_has_call(text, call) for call in ui_calls):
        required_permissions |= PERM_UI
        reasons.append("lvgl-ui")

    game_calls = (
        "proto_game_open_surface", "proto_game_query_capabilities",
        "proto_game_upload_asset",
        "proto_game_upload_palette", "proto_game_submit_frame",
        "proto_game_submit_frame_v2", "proto_game_destroy_asset",
        "proto_game_destroy_palette", "proto_gfx_init",
        "proto_3d_init", "proto_3d_query_caps", "proto_3d_exchange",
    )
    if any(_has_call(text, call) for call in game_calls):
        required_permissions |= PERM_UI
        reasons.append("game2d")

    audio_calls = (
        "proto_audio_open", "proto_audio_write", "proto_audio_set_volume",
        "proto_audio_tone", "proto_audio_flush", "proto_audio_stat",
    )
    if any(_has_call(text, call) for call in audio_calls):
        # Audio is permission-backed authority. Do not emit the raw device cap:
        # the loader derives KERNEL_CAP_DEVICE_AUDIO from the granted manifest
        # permission and deliberately strips untrusted raw audio grants.
        required_permissions |= PERM_AUDIO
        reasons.append("audio")

    net_calls = (
        "proto_net_status", "proto_net_tcp_connect",
        "proto_net_tcp_read", "proto_net_tcp_write",
        "proto_net_tcp_close",
    )
    if any(_has_call(text, call) for call in net_calls):
        required_permissions |= PERM_WIFI
        reasons.append("wifi")
    listen_calls = ("proto_net_tcp_listen", "proto_net_tcp_accept")
    if any(_has_call(text, call) for call in listen_calls):
        required_permissions |= PERM_WIFI | PERM_NETWORK_LISTEN
        reasons.extend(("wifi", "network-listen"))

    # Direct PIE assembly has no mediated SDK call to infer from. Recognize an
    # actual instruction -- a .S line, or the start of an inline-asm string in
    # C -- rather than a prose mention, and make the scheduling/register-context
    # permission visible in the generated manifest. build_app() keys the PIE
    # ISA extension off this same bit.
    if re.search(r'(?mi)(^\s*|"\s*)esp\.([a-z0-9_]+\.[a-z0-9_.]+(\s|\\n|"|$)|[a-z0-9_]+\s+q[0-7]\b)', text):
        required_permissions |= PERM_SIMD
        reasons.append("simd")

    dl_calls = ("proto_dlopen", "proto_dlsym", "proto_dlclose", "proto_dlerror")
    if any(_has_call(text, call) for call in dl_calls):
        reasons.append("dynamic-library")

    # Both SDK log entry points send to the logger; an app that only ever
    # calls proto_logf() was getting no logger target, and every line was
    # denied at the IPC policy check.
    if _has_call(text, "proto_log") or _has_call(text, "proto_logf"):
        required_permissions |= PERM_IPC_LOGGER
        reasons.append("log")
    if _has_call(text, "proto_send"):
        reasons.append("ipc-send")
    if _has_call(text, "proto_recv"):
        app_flags |= APP_FLAG_HEAVY_INPUT

    build_overrides: dict[str, str] = {}

    def apply_metadata_body(body: str):
        nonlocal caps, required_permissions, optional_permissions, app_flags
        tokens = re.split(r"[\s,]+", body)
        for token in tokens:
            if not token:
                continue
            lower = token.lower()
            if lower in ("hidden", "service", "background", "helper"):
                app_flags |= APP_FLAG_HIDDEN
                reasons.append("source-hidden")
                continue
            if lower in ("exit-hook", "on-exit"):
                app_flags |= APP_FLAG_EXIT_HOOK
                caps |= KERNEL_CAP_IPC_RECV
                reasons.append("source-exit-hook")
                continue
            if "=" not in token:
                raise SystemExit(f"unknown proto metadata token {token!r}")
            key, value = token.split("=", 1)
            key = key.strip().lower().replace("_", "-")
            if key == "package":
                string_overrides["package"] = value.strip()
                reasons.append("source-package")
                continue
            if key in ("optimize", "opt"):
                level = value.strip().lstrip("-")
                if level not in PROTOELF_OPT_LEVELS:
                    raise SystemExit(
                        f"unsupported optimize level {value.strip()!r}; "
                        f"choose one of {', '.join(sorted(PROTOELF_OPT_LEVELS))}"
                    )
                build_overrides["opt"] = level
                reasons.append("source-optimize")
                continue
            value_int = _parse_auto_int(value, key)
            if key in ("grant-caps", "caps"):
                caps |= value_int
                reasons.append("source-caps")
            elif key in ("required-permissions", "required-perms", "perms"):
                required_permissions |= value_int
                reasons.append("source-required-permissions")
            elif key in ("optional-permissions", "optional-perms"):
                optional_permissions |= value_int
                reasons.append("source-optional-permissions")
            elif key in ("app-flags", "flags"):
                app_flags |= value_int
                reasons.append("source-flags")
            elif key in ("heap", "stack", "quota", "ipc-targets", "listen-port",
                         "slot-span-log2", "large-heap-log2"):
                scalar_overrides[key.replace("-", "_")] = value_int
                reasons.append(f"source-{key}")
            else:
                raise SystemExit(f"unknown proto metadata key {key!r}")

    for match in re.finditer(r"(?mi)^\s*(?://|/\*)\s*proto:\s*(.+?)\s*(?:\*/)?$", text):
        apply_metadata_body(match.group(1).strip())
    for match in re.finditer(r'(?s)\bPROTO_APP_METADATA\s*\(\s*"([^"]*)"', text):
        apply_metadata_body(match.group(1).strip())

    return {
        "grant_caps": caps,
        "required_permissions": required_permissions,
        "optional_permissions": optional_permissions,
        "app_flags": app_flags,
        "scalars": scalar_overrides,
        "strings": string_overrides,
        "build": build_overrides,
        "reasons": sorted(set(reasons)),
        "source_count": len(files),
    }


def apply_auto_metadata(args):
    if getattr(args, "no_auto_caps", False):
        return
    app = getattr(args, "app", None)
    if app is None:
        return
    # build_app needs this before it compiles (optimize=), manifest() needs it
    # before it writes the manifest (caps, heap, ...), and build-bundle/deploy
    # run both against one namespace. Applying twice would be harmless -- caps
    # are OR-ed and scalars only fill defaults -- but it would print the
    # summary twice, so settle it once.
    if getattr(args, "_auto_metadata_applied", False):
        return
    args._auto_metadata_applied = True
    meta = detect_app_metadata(app)
    if not meta:
        return

    defaults = {
        "heap": PROTOELF_MIN_HEAP_BYTES,
        "stack": 4096,
        "quota": None,
        "ipc_targets": 0,
        "listen_port": 0,
    }
    for key, value in meta.get("scalars", {}).items():
        if hasattr(args, key) and getattr(args, key) == defaults.get(key, getattr(args, key)):
            setattr(args, key, value)
    for key, value in meta.get("strings", {}).items():
        if hasattr(args, key) and getattr(args, key) in (None, ""):
            setattr(args, key, value)
    # An explicit --opt on the command line wins; otherwise the app's own
    # declaration does. The global default stays -Os for every app that says
    # nothing, so one app opting into -O2 cannot grow every other artifact.
    build = meta.get("build", {})
    if "opt" in build and getattr(args, "opt", None) in (None, ""):
        args.opt = build["opt"]

    # The standalone `build` subcommand intentionally has no manifest
    # arguments, but it still calls this helper so source-declared optimize=
    # metadata is available before compilation. Keep the shared scanner safe
    # for that smaller argparse namespace; build-bundle/deploy already provide
    # these fields through add_manifest_args().
    for field in (
        "grant_caps",
        "required_permissions",
        "optional_permissions",
        "app_flags",
        "ipc_targets",
    ):
        if not hasattr(args, field):
            setattr(args, field, 0)

    args.grant_caps |= meta["grant_caps"]
    args.required_permissions |= meta["required_permissions"]
    args.optional_permissions |= meta["optional_permissions"]
    args.app_flags |= meta["app_flags"]
    if ((args.required_permissions | args.optional_permissions) &
            PERM_IPC_LOGGER) != 0 and args.ipc_targets == 0:
        args.ipc_targets = IPC_TARGET_LOGGER

    if meta["grant_caps"] or meta["required_permissions"] or meta["app_flags"] or meta["optional_permissions"]:
        reason_text = ", ".join(meta["reasons"]) if meta["reasons"] else "source scan"
        print(
            "[protelf auto] "
            f"{meta['source_count']} source files -> "
            f"grant-caps=0x{args.grant_caps:08X} "
            f"required-permissions=0x{args.required_permissions:X} "
            f"optional-permissions=0x{args.optional_permissions:X} "
            f"app-flags=0x{args.app_flags:X} "
            f"({reason_text})"
        )


def parse_library_specs(values: list[str] | None) -> list[tuple[str, Path, bool]]:
    specs = []
    for raw in values or []:
        optional = raw.startswith("?")
        if optional:
            raw = raw[1:]
        if "=" not in raw:
            raise SystemExit("--library expects SONAME=PATH or ?SONAME=PATH")
        soname, path_text = raw.split("=", 1)
        path = rel(Path(path_text))
        if not soname or any(ch not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-+" for ch in soname):
            raise SystemExit(f"invalid library SONAME: {soname!r}")
        if not path.exists():
            raise SystemExit(f"library not found: {path}")
        validate_elf(path, dynamic_ok=True, library=True)
        actual = elf_dynamic_info(path, library=True)["soname"]
        if actual != soname:
            raise SystemExit(f"{path}: SONAME is {actual!r}, expected {soname!r}")
        specs.append((soname, path, optional))
    if len(specs) + 1 > MAX_OBJECTS:
        raise SystemExit(f"ProtoELF supports at most {MAX_OBJECTS} objects including the app")
    if len({soname for soname, _path, _optional in specs}) != len(specs):
        raise SystemExit("duplicate library SONAME")
    return specs


def write_object_table(app_id: int, specs: list[tuple[str, Path, bool]], out: Path) -> Path:
    header = struct.pack(
        "<6I8s",
        PROTO_OBJECT_MAGIC,
        PROTO_OBJECT_VERSION,
        PROTO_OBJECT_HEADER_SIZE,
        PROTO_OBJECT_ENTRY_SIZE,
        len(specs),
        0,
        b"\0" * 8,
    )
    rows = bytearray()
    for index, (soname, path, optional) in enumerate(specs, start=1):
        blob = path.read_bytes()
        flags = (PROTO_OBJECT_OPTIONAL if optional else PROTO_OBJECT_REQUIRED)
        if not optional:
            flags |= PROTO_OBJECT_GLOBAL
        stage_name = f"{app_id}.l{index}"
        rows.extend(
            struct.pack(
                "<6I32s48s48s",
                index,
                flags,
                PROTO_ELF_ABI_VERSION,
                0,
                len(blob),
                0,
                hashlib.sha256(blob).digest(),
                soname.encode("ascii") + b"\0" * (48 - len(soname)),
                stage_name.encode("ascii") + b"\0" * (48 - len(stage_name)),
            )
        )
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(header + rows)
    print(f"objects {out}")
    return out


def build_app(args) -> Path:
    app_dir = rel(args.app)
    if not app_dir.exists():
        raise SystemExit(f"app directory not found: {app_dir}")
    name = args.name or app_dir.name
    out_dir = rel(args.out_dir or Path("build") / f"protelf_{fixed_app_stem(name)}")
    profile = getattr(args, "sdk_profile", "full")
    sources = [REPO_ROOT / path for path in sdk_sources(profile)]
    sdk_archives = sdk_libraries(profile)
    missing = [str(path.relative_to(REPO_ROOT)) for path in sources if not path.is_file()]
    if missing:
        raise SystemExit(
            f"SDK profile {profile!r} is incomplete: {', '.join(missing)}. "
            "The standalone core SDK requires --sdk-profile core.")
    out_dir.mkdir(parents=True, exist_ok=True)
    out_elf = out_dir / "app.elf"
    sdk_count = len(sources)
    sources.extend(sorted(app_dir.glob("*.c")))
    if len(sources) <= sdk_count:
        raise SystemExit(f"no app .c sources found in {app_dir}")
    # Hand-written assembly (e.g. PIE kernels). A set, because Windows globs
    # are case-insensitive and "*.S" also matches "*.s".
    asm_sources = sorted({p.resolve() for p in app_dir.glob("*.S")} |
                         {p.resolve() for p in app_dir.glob("*.s")})
    gcc = find_gcc(args.gcc)
    # Before the compile, so an app's `optimize=` declaration is in hand.
    apply_auto_metadata(args)
    opt_level = (getattr(args, "opt", None) or PROTOELF_DEFAULT_OPT).lstrip("-")
    if opt_level not in PROTOELF_OPT_LEVELS:
        raise SystemExit(
            f"unsupported optimize level {opt_level!r}; "
            f"choose one of {', '.join(sorted(PROTOELF_OPT_LEVELS))}"
        )
    if opt_level != PROTOELF_DEFAULT_OPT:
        print(f"[protelf] {name}: building at -{opt_level} "
              f"(default is -{PROTOELF_DEFAULT_OPT})")
    libraries = parse_library_specs(getattr(args, "library", None))
    # C always builds for the portable ISA, so it is identical on both P4
    # families. SIMD lives only in .S files, which are built twice -- once per
    # PIE encoding -- and selected at startup (build_simd_dual_encoding()). An
    # app without the SIMD permission never gets a PIE extension at all, so it
    # cannot assemble an instruction the kernel would fault on.
    march = PROTOELF_MARCH
    link_inputs = list(asm_sources)
    defsyms: list[str] = []
    if args.required_permissions & PERM_SIMD:
        link_inputs, defsyms = build_simd_dual_encoding(
            gcc, name, app_dir, sources, asm_sources, out_dir)
    sources.extend(link_inputs)
    cmd = [
        gcc,
        "-I",
        REPO_ROOT / "main" / "include",
        "-I",
        REPO_ROOT / "loaded_apps" / "sdk",
        "-I",
        app_dir,
        "-I",
        app_dir / "include",
        "-ffreestanding",
        "-fno-builtin",
        "-fvisibility=hidden",
        "-fPIE",
        f"-{opt_level}",
        "-ffunction-sections",
        "-fdata-sections",
        # Match ESP-IDF's ESP32-P4 ISA/ABI.  The compiler driver's generic
        # default includes RV32D, but P4 only implements RV32F; omitting these
        # flags can ship unsupported double-precision opcodes in a ProtoELF.
        f"-march={march}",
        "-mabi=ilp32f",
        "-msmall-data-limit=0",
        "-mno-relax",
        "-nostdlib",
        "-Wl,-pie",
        "-Wl,--gc-sections",
        "-Wl,--no-relax",
        "-Wl,--hash-style=sysv",
        "-Wl,-z,max-page-size=256",
        # A released ProtoELF ships no debug info; the symbol/string tables
        # a plain link leaves in are pure ELF-file overhead against the
        # 128 KiB slot budget, not part of what actually loads (PT_LOAD only
        # covers text/data). Stripping recovers that headroom for free.
        "-Wl,-s",
        # ABI 5 links against the fixed 0x08000000 flash->PSRAM bias so the
        # image can execute in place; ABI 4 keeps the RAM-loader layout.
        f"-Wl,-T,{_linker_script(getattr(args, 'abi', PROTO_ELF_ABI_VERSION))}",
        "-Wl,-e,_proto_start",
        *sources,
    ]
    if sdk_archives:
        # Startup must be extracted even when the app makes no SDK calls;
        # archive rescanning resolves freestanding helpers referenced by graphics.
        cmd.extend(["-Wl,-u,_proto_start", "-Wl,--start-group", *sdk_archives, "-Wl,--end-group"])
    cmd.append("-Wl,--dynamic-linker=/sys/protold.so")
    cmd.extend(f"-Wl,--defsym={entry}" for entry in defsyms)
    if libraries:
        cmd.append("-Wl,--no-as-needed")
        cmd.extend(path for _soname, path, optional in libraries if not optional)
    cmd.extend(["-o", out_elf])
    run_cmd(cmd)
    validate_elf(out_elf, dynamic_ok=True)
    print(f"built {out_elf}")
    return out_elf


# The two ESP32-P4 SIMD encodings. The same mnemonics assemble to different
# words: v1.x is `xesppie`, v3.x is `xespv` with hardware loops split out as
# `xesploop` (docs/hardware-support.md). The suffixes name each copy.
SIMD_ENCODINGS = (
    ("pie_v1", PROTOELF_MARCH + "_xesppie"),
    ("pie_v3", PROTOELF_MARCH + "_xesploop_xespv"),
)
_SIMD_TEXT_RE = re.compile(
    r'(?mi)(^\s*|"\s*)esp\.([a-z0-9_]+\.[a-z0-9_.]+(\s|\\n|"|$)|[a-z0-9_]+\s+q[0-7]\b)')


def _binutil(gcc: str, tool: str) -> str:
    path = Path(gcc)
    # shutil.which() may return "...gcc.EXE" on Windows.
    lower = path.name.lower()
    exe = lower.endswith(".exe")
    base = path.name[:-4] if exe else path.name
    if not base.lower().endswith("gcc"):
        raise SystemExit(f"can't derive binutils from compiler name {path.name}")
    candidate = path.with_name(base[:-3] + tool + (".exe" if exe else ""))
    if not candidate.exists():
        raise SystemExit(f"{candidate.name} not found next to {path}")
    return str(candidate)


def _global_symbols(readelf: str, obj: Path) -> list[tuple[str, str]]:
    """(name, type) for every global symbol defined in `obj`."""
    out = subprocess.run([readelf, "-sW", str(obj)], check=True,
                         capture_output=True, text=True).stdout
    symbols = []
    for line in out.splitlines():
        parts = line.split()
        # Num: Value Size Type Bind Vis Ndx Name
        if len(parts) == 8 and parts[4] == "GLOBAL" and parts[6] != "UND":
            symbols.append((parts[7], parts[3]))
    return symbols


def build_simd_dual_encoding(gcc: str, name: str, app_dir: Path,
                             c_sources: list[Path], asm_sources: list[Path],
                             out_dir: Path) -> tuple[list[Path], list[str]]:
    """Assemble every .S once per SIMD encoding; return link inputs.

    Each exported function `foo` becomes `foo__pie_v1` and `foo__pie_v3`, and a
    generated stub named `foo` tail-jumps through a slot that
    proto_simd_select() fills once at startup (loaded_apps/sdk/proto_loaded.c)
    from the kernel's PROTO_ELF_LOAD_MAP_FLAG_PIE_XESPV. Callers keep calling
    `foo`; the cost is one indirect jump per call plus one copy of the routine.
    """
    for source in c_sources:
        if source.parent == app_dir and _SIMD_TEXT_RE.search(
                source.read_text(encoding="utf-8", errors="replace")):
            raise SystemExit(
                f"{source}: SIMD instructions in C inline assembly can't be built "
                "for both ESP32-P4 encodings; move them into a .S file")
    if not asm_sources:
        raise SystemExit(f"{name}: SIMD permission but no .S sources to build")
    readelf = _binutil(gcc, "readelf")
    objcopy = _binutil(gcc, "objcopy")
    simd_dir = out_dir / "simd"
    simd_dir.mkdir(parents=True, exist_ok=True)
    objects: list[Path] = []
    functions: list[str] = []
    markers: list[str] = []
    for source in asm_sources:
        exported: list[tuple[str, str]] | None = None
        for suffix, march in SIMD_ENCODINGS:
            obj = simd_dir / f"{source.stem}.{suffix}.o"
            run_cmd([gcc, "-c", "-I", REPO_ROOT / "main" / "include",
                     "-I", REPO_ROOT / "loaded_apps" / "sdk", "-I", app_dir,
                     f"-march={march}", "-mabi=ilp32f", "-mno-relax",
                     source, "-o", obj])
            symbols = _global_symbols(readelf, obj)
            if exported is None:
                exported = symbols
            elif sorted(symbols) != sorted(exported):
                raise SystemExit(f"{source}: the two SIMD encodings export different symbols")
            renames = [f"--redefine-sym={sym}={sym}__{suffix}" for sym, _type in symbols]
            if renames:
                run_cmd([objcopy, *renames, obj, obj])
            objects.append(obj)
        for sym, sym_type in exported or []:
            # A label with no `.type` is NOTYPE; it may well be called from C,
            # so it gets a stub too (a stub nobody calls is harmless). Only an
            # explicit data object is bound to one copy.
            (markers if sym_type == "OBJECT" else functions).append(sym)
    if len(set(functions + markers)) != len(functions + markers):
        raise SystemExit(f"{name}: a SIMD symbol is defined in more than one .S file")
    if not functions:
        raise SystemExit(f"{name}: no exported SIMD routines found in the .S files")

    lines = [
        "/* Generated by tools/protelf_dev.py: SIMD dispatch for both ESP32-P4",
        " * encodings. Do not edit. */",
        "    .option norelax",
        "    .section .data.proto_simd_slots, \"aw\", @progbits",
        "    .balign 4",
    ]
    for sym in functions:
        lines += [f"proto_simd_slot_{sym}:", "    .word 0"]
    lines += ["    .section .text.proto_simd_dispatch, \"ax\", @progbits", "    .balign 4"]
    for sym in functions:
        # A tail jump keeps a0-a7 and ra intact; t0 is caller-saved.
        lines += [
            f"    .global {sym}",
            f"    .type {sym}, @function",
            f"{sym}:",
            f"    lla t0, proto_simd_slot_{sym}",
            "    lw t0, 0(t0)",
            "    jr t0",
            f"    .size {sym}, .-{sym}",
        ]
    lines += [
        "    .global proto_simd_select",
        "    .type proto_simd_select, @function",
        "proto_simd_select:",
        "    bnez a0, 2f",
    ]
    for suffix_label, suffix in (("1", "pie_v1"), ("2", "pie_v3")):
        lines.append(f"{suffix_label}:")
        for sym in functions:
            lines += [
                f"    lla t1, {sym}__{suffix}",
                f"    lla t0, proto_simd_slot_{sym}",
                "    sw t1, 0(t0)",
            ]
        lines.append("    ret")
    lines.append("    .size proto_simd_select, .-proto_simd_select")
    dispatch = simd_dir / "proto_simd_dispatch.S"
    dispatch.write_text("\n".join(lines) + "\n", encoding="ascii")
    print(f"[protelf] {name}: SIMD built for both P4 encodings "
          f"({', '.join(functions)}); selected at startup")
    # Data objects keep their name, bound to the v1 copy; both copies hold the
    # same bytes for data.
    defsyms = [f"{sym}={sym}__pie_v1" for sym in markers]
    return objects + [dispatch], defsyms


def _linker_script(abi: int) -> Path:
    """Pick the linker script that matches the requested loader ABI."""
    sdk = REPO_ROOT / "loaded_apps" / "sdk"
    if abi == PROTO_ELF_ABI_VERSION:
        return sdk / "protelf_app.ld"
    if abi == PROTO_ELF_ABI_VERSION_V4:
        return sdk / "protelf_app_abi4.ld"
    raise SystemExit(f"unsupported loader ABI {abi}")


def build_library(args) -> Path:
    source_dir = rel(args.lib)
    if not source_dir.exists():
        raise SystemExit(f"library directory not found: {source_dir}")
    sources = sorted(source_dir.glob("*.c"))
    if not sources:
        raise SystemExit(f"no library .c sources found in {source_dir}")
    out_dir = rel(args.out_dir or Path("build") / f"protelf_lib_{fixed_app_stem(args.soname)}")
    out_dir.mkdir(parents=True, exist_ok=True)
    out = out_dir / args.soname
    gcc = find_gcc(args.gcc)
    libraries = parse_library_specs(getattr(args, "library", None))
    if any(optional for _soname, _path, optional in libraries):
        raise SystemExit("build-lib dependencies cannot be optional")
    cmd = [
        gcc,
        "-I",
        REPO_ROOT / "main" / "include",
        "-I",
        REPO_ROOT / "loaded_apps" / "sdk",
        "-ffreestanding",
        "-fno-builtin",
        "-fvisibility=hidden",
        "-fPIC",
        "-Os",
        "-ffunction-sections",
        "-fdata-sections",
        f"-march={PROTOELF_MARCH}",
        "-mabi=ilp32f",
        "-msmall-data-limit=0",
        "-mno-relax",
        "-nostdlib",
        "-shared",
        "-Wl,--gc-sections",
        "-Wl,--no-relax",
        "-Wl,--hash-style=sysv",
        "-Wl,-z,max-page-size=256",
        # See build_app(): strips symbol/string-table overhead a released
        # binary has no use for, recovering headroom against the slot budget.
        "-Wl,-s",
        f"-Wl,-T,{REPO_ROOT / 'loaded_apps' / 'sdk' / 'protelf_lib.ld'}",
        f"-Wl,-soname,{args.soname}",
        *sources,
    ]
    if libraries:
        cmd.append("-Wl,--no-as-needed")
        cmd.extend(path for _soname, path, _optional in libraries)
    cmd.extend(["-o", out])
    run_cmd(cmd)
    validate_elf(out, dynamic_ok=True, library=True)
    info = elf_dynamic_info(out, library=True)
    if info["soname"] != args.soname:
        raise SystemExit("linker did not emit the requested SONAME")
    print(f"built library {out}")
    return out


def manifest(args, elf: Path | None = None, objects: Path | None = None) -> Path:
    elf = rel(elf or args.elf)
    objects = rel(objects) if objects is not None else (
        rel(args.objects) if getattr(args, "objects", None) else None
    )
    if objects is None:
        raise SystemExit("ProtoELF ABI 4 manifests require an objects.pobj table")
    validate_elf(elf, dynamic_ok=True)
    abi = getattr(args, "abi", PROTO_ELF_ABI_VERSION)
    name = fixed_app_stem(args.name or elf.stem).lower()
    apply_auto_metadata(args)
    large_heap_log2 = getattr(args, "large_heap_log2", 0)
    if large_heap_log2 != 0 and not PROTO_ELF_LARGE_HEAP_LOG2_MIN <= large_heap_log2 <= PROTO_ELF_LARGE_HEAP_LOG2:
        raise SystemExit(
            f"large-heap-log2 must be 0 or {PROTO_ELF_LARGE_HEAP_LOG2_MIN}..{PROTO_ELF_LARGE_HEAP_LOG2}"
        )
    if getattr(args, "quota", None) is None:
        args.quota = auto_loaded_quota(elf, args.heap, abi,
                                       getattr(args, "slot_span_log2", 0))
        slots = 1 << getattr(args, "slot_span_log2", 0)
        if large_heap_log2:
            args.quota += 1 << large_heap_log2
        print(f"[protelf auto] quota={args.quota} bytes from ELF RW window "
              f"({slots} slot(s))"
              + (" + large heap" if large_heap_log2 else ""))
    warn_loaded_layout(elf, args.heap, name, abi,
                       getattr(args, "slot_span_log2", 0))
    out_arg = getattr(args, "out", None)
    out = rel(out_arg or elf.with_name("manifest.pmanifest"))
    cmd = [
        sys.executable,
        REPO_ROOT / "tools" / "protelf_manifest.py",
        "--elf",
        elf,
        "--out",
        out,
        "--app-id",
        str(args.app_id),
        "--pid",
        str(args.pid),
        "--name",
        name,
        "--package",
        args.package or f"dev.proto.{package_stem(name)}",
        "--display",
        args.display or name,
        "--version",
        args.version,
        "--version-code",
        str(args.version_code),
        "--stack",
        str(args.stack),
        "--quota",
        str(args.quota),
        "--heap",
        str(args.heap),
        "--grant-caps",
        str(args.grant_caps),
        "--deny-caps",
        str(args.deny_caps),
        "--app-flags",
        str(args.app_flags),
        "--ipc-targets",
        str(args.ipc_targets),
        "--required-permissions",
        str(args.required_permissions),
        "--optional-permissions",
        str(args.optional_permissions),
        "--granted-permissions",
        str(args.granted_permissions),
        "--denied-permissions",
        str(args.denied_permissions),
        "--listen-port",
        str(args.listen_port),
        "--slot-span-log2",
        str(getattr(args, "slot_span_log2", 0)),
        "--large-heap-log2",
        str(large_heap_log2),
    ]
    if objects is not None:
        cmd.extend(["--objects", objects])
    table = getattr(args, "resource_table", None)
    payload = getattr(args, "resource_payload", None)
    if table is not None and payload is not None:
        cmd.extend(["--resource-table", table, "--resource-payload", payload])
    if getattr(args, "abi", PROTO_ELF_ABI_VERSION) == PROTO_ELF_ABI_VERSION:
        cmd.append("--flash-xip")
    run_cmd(cmd)
    print(f"manifest {out}")
    return out


def sign_app(args) -> Path:
    elf = rel(args.elf)
    manifest_path = rel(args.manifest)
    objects = rel(args.objects)
    validate_elf(elf, dynamic_ok=True)
    out = rel(args.out or elf.with_name("app.sig"))
    manifest_data = manifest_path.read_bytes()
    elf_data = elf.read_bytes()
    object_blob = objects.read_bytes()
    try:
        record_hashes = validate_bundle_components(
            manifest_data, elf_data, object_blob, release_signing=True
        )
    except ValueError as exc:
        raise SystemExit(f"ProtoELF release signing refused bundle: {exc}") from exc
    manifest_digest = hashlib.sha256(manifest_data).digest()
    elf_digest = hashlib.sha256(elf_data).digest()
    digest = hashlib.sha256(
        manifest_digest
        + elf_digest
        + hashlib.sha256(object_blob).digest()
        + b"".join(record_hashes)
    ).digest()
    try:
        from cryptography.hazmat.primitives import hashes, serialization
        from cryptography.hazmat.primitives.asymmetric import ec, utils
    except ImportError as exc:
        raise SystemExit(
            "cryptography is required for signing: python -m pip install cryptography"
        ) from exc

    key_data = rel(args.key).read_bytes()
    key = serialization.load_pem_private_key(
        key_data,
        password=args.key_password.encode("utf-8") if args.key_password else None,
    )
    if not isinstance(key, ec.EllipticCurvePrivateKey) or key.curve.name != "secp256r1":
        raise SystemExit("ProtoELF release signing requires an ECDSA P-256 PEM private key")
    sig = key.sign(digest, ec.ECDSA(utils.Prehashed(hashes.SHA256())))
    if len(sig) > 72:
        raise SystemExit(f"signature too large for ABI: {len(sig)} bytes")
    out.write_bytes(sig)
    print(f"signed {elf} -> {out}")
    return out


class ProtoSerial:
    def __init__(self, port: str, baud: int, timeout: float):
        serial = import_serial()
        try:
            self.ser = serial.Serial()
            self.ser.port = port
            self.ser.baudrate = baud
            self.ser.timeout = 0.1
            self.ser.write_timeout = timeout
            self.ser.dtr = False
            self.ser.rts = False
            self.ser.open()
        except serial.SerialException as exc:
            raise SystemExit(
                f"could not open {port}: {exc}\n"
                "Close any serial monitor/ESP-IDF monitor/PuTTY/VS Code terminal using that COM port."
            ) from exc
        self.timeout = timeout
        self._line_buf = bytearray()

    def close(self):
        self.ser.close()

    def write_line(self, line: str, char_delay: float = 0.0):
        if char_delay <= 0.0:
            self.ser.write((line + "\r\n").encode("ascii"))
            self.ser.flush()
            return
        for ch in line:
            self.ser.write(ch.encode("ascii"))
            self.ser.flush()
            time.sleep(char_delay)
        self.ser.write(b"\r\n")
        self.ser.flush()

    def read_line(self, timeout: float | None = None) -> str:
        deadline = time.monotonic() + (timeout if timeout is not None else self.timeout)
        while time.monotonic() < deadline:
            ch = self.ser.read(1)
            if not ch:
                continue
            if ch in (b"\r", b"\n"):
                if self._line_buf:
                    line = self._line_buf.decode("utf-8", errors="replace")
                    self._line_buf.clear()
                    return line
                continue
            self._line_buf.extend(ch)
        raise TimeoutError("serial read timed out")

    def read_until(self, predicate, timeout: float | None = None) -> str:
        deadline = time.monotonic() + (timeout if timeout is not None else self.timeout)
        while time.monotonic() < deadline:
            line = self.read_line(max(0.2, deadline - time.monotonic()))
            print(line)
            if predicate(line):
                return line
        raise TimeoutError("serial wait timed out")

    def read_available_lines(self, duration: float) -> list[str]:
        deadline = time.monotonic() + duration
        lines: list[str] = []
        while time.monotonic() < deadline:
            try:
                line = self.read_line(max(0.05, deadline - time.monotonic()))
            except TimeoutError:
                break
            print(line)
            lines.append(line)
        return lines

    def command_until(self, command: str, predicate, timeout: float | None = None,
                      char_delay: float = 0.0, shell_guard: bool = False) -> str:
        if shell_guard:
            command = " " + command
        self.write_line(command, char_delay)
        return self.read_until(predicate, timeout)


def upload_file(args, local: Path | None = None, remote: str | None = None, ps: ProtoSerial | None = None):
    local = rel(local or args.file)
    remote = remote or args.remote
    blob = local.read_bytes()
    digest = hashlib.sha256(blob).hexdigest()
    chunk = min(args.chunk, DEFAULT_CHUNK)
    if chunk <= 0 or chunk > DEFAULT_CHUNK:
        raise SystemExit(f"chunk must be 1..{DEFAULT_CHUNK}")
    own_serial = ps is None
    ps = ps or ProtoSerial(args.port, args.baud, args.timeout)
    try:
        if own_serial:
            login_if_requested(args, ps)
        ready = ps.command_until(
            f"recv {remote} {len(blob)} {digest}",
            lambda line: line.startswith("OK READY") or line.startswith("recv:") or line.startswith("ERR"),
            char_delay=args.shell_char_delay,
            shell_guard=True,
        )
        if not ready.startswith("OK READY"):
            raise SystemExit(f"upload refused: {ready}")
        seq = 0
        for off in range(0, len(blob), chunk):
            piece = blob[off : off + chunk]
            crc = binascii.crc32(piece) & 0xFFFFFFFF
            b64 = base64.b64encode(piece).decode("ascii")
            ps.write_line(f"DATA {seq} {len(piece)} {crc:08x} {b64}")
            line = ps.read_until(lambda line: line == f"OK {seq}" or line.startswith("ERR"), args.timeout)
            if line != f"OK {seq}":
                raise SystemExit(f"upload failed: {line}")
            seq += 1
        ps.write_line(f"DONE {digest}")
        line = ps.read_until(lambda line: line == "DONE ok" or line.startswith("ERR"), args.timeout)
        if line != "DONE ok":
            raise SystemExit(f"upload failed: {line}")
        time.sleep(args.after_command_delay)
    finally:
        if own_serial:
            ps.close()
    print(f"uploaded {local} -> {remote}")


def serial_command(args, command: str, done_prefix: str | None = None, ps: ProtoSerial | None = None):
    own_serial = ps is None
    ps = ps or ProtoSerial(args.port, args.baud, args.timeout)
    try:
        if own_serial:
            login_if_requested(args, ps)
        if done_prefix is None:
            ps.write_line(command, args.shell_char_delay)
            time.sleep(args.after_command_delay)
            return
        ps.command_until(command, lambda line: line.startswith(done_prefix),
                         args.timeout, args.shell_char_delay, shell_guard=True)
    finally:
        if own_serial:
            ps.close()


def login_if_requested(args, ps: ProtoSerial):
    password = getattr(args, "login_password", None)
    user = getattr(args, "login_user", None)

    if not password:
        return

    # Opening the ESP USB serial port commonly resets the board. Do not submit
    # credentials until either the terminal announces readiness or the port
    # has remained quiet long enough that it was already waiting for input.
    deadline = time.monotonic() + args.timeout
    quiet_deadline = time.monotonic() + 1.0
    saw_boot_output = False
    saw_terminal_ready = False
    while time.monotonic() < deadline:
        try:
            line = ps.read_line(0.25)
        except TimeoutError:
            if b"proto:" in ps._line_buf:
                return
            if not saw_boot_output and time.monotonic() >= quiet_deadline:
                break
            continue
        print(line)
        if "proto:" in line or line == "login ok":
            return
        if line == "PROTO serial terminal ready" or line.startswith("login "):
            saw_terminal_ready = line == "PROTO serial terminal ready"
            break
        saw_boot_output = True

    if not saw_terminal_ready:
        # A reconnect may preserve an authenticated shell. Complete its
        # unterminated prompt with a blank line before deciding to log in.
        ps.write_line("")
        probe = ps.read_available_lines(0.75)
        if any("proto:" in line for line in probe) or b"proto:" in ps._line_buf:
            return

    if user:
        ps.write_line(user, args.shell_char_delay)
        time.sleep(0.2)
    ps.write_line(password, args.shell_char_delay)
    ps.read_until(
        lambda line: line == "login ok" or line.startswith("login "),
        args.timeout,
    )


def write_resource_sections(specs, out_dir: Path):
    """Pack --resource NAME=PATH specs into the .prt / .bin section pair.

    Returns (None, None) when the app declares no resources, which keeps the
    bundle byte-identical to a three-section ABI-4 build.
    """
    if not specs:
        return None, None
    entries = protelf_resources.parse_resource_specs(specs)
    table, payload = protelf_resources.pack_resources(entries)
    out_dir.mkdir(parents=True, exist_ok=True)
    table_path = out_dir / "resources.prt"
    payload_path = out_dir / "resources.bin"
    table_path.write_bytes(table)
    payload_path.write_bytes(payload)
    rows = protelf_resources.validate_resources(table, payload)
    print(f"resources {len(rows)} entries  table {len(table):,} B  "
          f"payload {len(payload):,} B")
    return table_path, payload_path


def build_bundle(args) -> Path:
    """Build ELF + manifest + pack into a single .pelf bundle. No serial needed."""
    libraries = parse_library_specs(getattr(args, "library", None))
    elf = build_app(args)
    name = args.name or rel(args.app).name
    out_dir = rel(args.out_dir or Path("build") / f"protelf_{fixed_app_stem(name)}")
    objects_path = write_object_table(args.app_id, libraries, out_dir / "objects.pobj")
    table_path, payload_path = write_resource_sections(
        getattr(args, "resource", None), out_dir
    )
    import types
    manifest_args = types.SimpleNamespace(**vars(args))
    manifest_args.out = out_dir / "manifest.pmanifest"
    manifest_args.resource_table = table_path
    manifest_args.resource_payload = payload_path
    manifest_path = manifest(manifest_args, elf, objects_path)
    out_pelf = rel(args.out) if getattr(args, "out", None) else Path(f"{fixed_app_stem(name)}.pelf")

    _pack_args = types.SimpleNamespace(
        manifest=manifest_path, elf=elf, objects=objects_path, out=out_pelf,
        resource_table=table_path, resource_payload=payload_path,
    )
    bundle = pack_bundle(_pack_args)
    print(f"\nReady to install: {bundle}")
    print("Drop this file into the ProtoELF Installer GUI or copy it to the device /shared/downloads/ folder.")
    return bundle


def deploy(args):
    libraries = parse_library_specs(getattr(args, "library", None))
    elf = build_app(args)
    name = args.name or rel(args.app).name
    out_dir = rel(args.out_dir or Path("build") / f"protelf_{fixed_app_stem(name)}")
    objects_path = write_object_table(
        args.app_id, libraries, out_dir / "objects.pobj"
    )
    # The serial install path uploads discrete files rather than a bundle, and
    # the device's dev-install entry point takes manifest+ELF+objects only. So
    # refuse here rather than silently dropping the resource sections and
    # shipping a manifest whose resource hash points at nothing on the device.
    if getattr(args, "resource", None):
        raise SystemExit(
            "deploy: --resource is not supported over serial; use "
            "'build-bundle' and install the .pelf instead"
        )
    args.resource_table = None
    args.resource_payload = None
    manifest_path = manifest(args, elf, objects_path)
    # Keep the initial recv command under the terminal's normal line limit.
    # Once recv mode starts, DATA lines use the larger upload-only reader.
    remote_stem = str(args.app_id)
    remote_elf = args.remote_elf or f"/shared/tmp/{remote_stem}.e"
    remote_manifest = args.remote_manifest or f"/shared/tmp/{remote_stem}.m"
    remote_objects = args.remote_objects or f"/shared/tmp/{remote_stem}.o"
    replace_install = not args.no_upgrade
    ps = ProtoSerial(args.port, args.baud, args.timeout)
    try:
        login_if_requested(args, ps)
        upload_file(args, manifest_path, remote_manifest, ps)
        upload_file(args, elf, remote_elf, ps)
        if objects_path is not None:
            upload_file(args, objects_path, remote_objects, ps)
            for index, (_soname, library, _optional) in enumerate(libraries, start=1):
                upload_file(
                    args,
                    library,
                    f"/shared/tmp/{args.app_id}.l{index}",
                    ps,
                )
        if args.sig:
            sig_path = rel(args.sig)
            remote_sig = args.remote_sig or f"/shared/tmp/{fixed_app_stem(name)}.sig"
            upload_file(args, sig_path, remote_sig, ps)
            replace = " --replace" if replace_install else ""
            object_arg = f" {remote_objects}" if objects_path is not None else ""
            serial_command(args, f"install-signed{replace} {remote_manifest} {remote_elf}{object_arg} {remote_sig}", "install-signed:", ps)
        else:
            replace = " --replace" if replace_install else ""
            object_arg = f" {remote_objects}" if objects_path is not None else ""
            serial_command(args, f"install-dev{replace} {remote_manifest} {remote_elf}{object_arg}", "install-dev:", ps)
        if args.run:
            serial_command(args, f"run {name}", "run", ps)
    finally:
        ps.close()


def add_serial_args(parser):
    parser.add_argument("--port", required=True, help="serial port, for example COM7")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=20.0)
    parser.add_argument("--chunk", type=int, default=DEFAULT_CHUNK)
    parser.add_argument("--after-command-delay", type=float, default=0.5)
    parser.add_argument("--shell-char-delay", type=float, default=0.01,
                        help="delay between chars for normal terminal commands")
    parser.add_argument("--login-user", default=None,
                        help="optional username to send before login password")
    parser.add_argument("--login-password", default=None,
                        help="optional password to log in before sending commands")


def add_manifest_args(parser):
    parser.add_argument(
        "--abi", type=int, default=PROTO_ELF_ABI_VERSION,
        choices=sorted(PROTO_ELF_ABI_VERSIONS_SUPPORTED),
        help=(f"loader ABI: {PROTO_ELF_ABI_VERSION} executes text in place from "
              f"flash, {PROTO_ELF_ABI_VERSION_V4} loads it into RAM"))
    parser.add_argument(
        "--resource", action="append", default=[], metavar="NAME=PATH",
        help=("ship a bulk asset in the signed resource section instead of the "
              "ELF; repeatable, ABI 5 only"))
    parser.add_argument("--app-id", required=True, type=int)
    parser.add_argument("--pid", type=int, default=0,
                        help="preferred runtime PID hint; 0 means dynamic")
    parser.add_argument("--name", required=True)
    parser.add_argument("--package", default=None)
    parser.add_argument("--display", default=None)
    parser.add_argument("--version", default="protelf-1")
    parser.add_argument(
        "--version-code",
        type=int,
        default=1,
        help="monotonic release code embedded in the signed manifest",
    )
    parser.add_argument("--stack", type=int, default=4096)
    parser.add_argument("--quota", type=parse_auto_int, default=None,
                        help="runtime proto_malloc budget; default auto from the ELF RW window")
    parser.add_argument("--heap", type=int, default=4096)
    parser.add_argument("--grant-caps", type=lambda s: int(s, 0), default=0)
    parser.add_argument("--deny-caps", type=lambda s: int(s, 0), default=0)
    parser.add_argument("--app-flags", type=lambda s: int(s, 0), default=0,
                        help="App flags bitmask, e.g. 0x10 heavy input, 0x8 hidden/service")
    parser.add_argument(
        "--ipc-targets",
        type=lambda s: int(s, 0),
        default=0,
        help=("allowed destination PID bitmask; source auto-detection adds only "
              "the logger target when ipc.logger is declared"),
    )
    parser.add_argument("--required-permissions", type=lambda s: int(s, 0), default=0)
    parser.add_argument("--optional-permissions", type=lambda s: int(s, 0), default=0)
    parser.add_argument("--granted-permissions", type=lambda s: int(s, 0), default=0)
    parser.add_argument("--denied-permissions", type=lambda s: int(s, 0), default=0)
    parser.add_argument("--listen-port", type=int, default=0,
                        help="declared inbound TCP port; 0 permits ephemeral only")
    parser.add_argument(
        "--slot-span-log2", type=int, default=0,
        choices=tuple(range(PROTELF_XIP_SPAN_LOG2_MAX + 1)),
        help=("flash-XIP arena run as a power-of-two slot count exponent: "
              "0=1 slot, 1=2 slots, 2=4 slots, 3=8 slots, 4=16 slots. "
              "Installation requires a free naturally aligned contiguous run"))
    parser.add_argument(
        "--large-heap-log2", type=int, default=0,
        choices=(0, *range(PROTO_ELF_LARGE_HEAP_LOG2_MIN, PROTO_ELF_LARGE_HEAP_LOG2 + 1)),
        help="request a private slice of the shared large heap: 0=none, "
             "16=64 KiB through 23=8 MiB; 22=4 MiB")
    parser.add_argument("--no-auto-caps", action="store_true",
                        help="disable source scanning; use exactly the manifest bits passed on the command line")


def bundle_objects(args) -> Path:
    specs = parse_library_specs(args.library)
    if not specs:
        raise SystemExit("bundle requires at least one --library")
    return write_object_table(args.app_id, specs, rel(args.out))


def inspect_elf(args):
    path = rel(args.elf)
    info = elf_dynamic_info(path, library=args.library)
    print(f"ELF: {path}")
    print(f"SONAME: {info['soname'] or '-'}")
    print("NEEDED: " + (", ".join(info["needed"]) if info["needed"] else "-"))
    print("Layout ABI: split-dynamic")
    for index, load in enumerate(info["loads"]):
        flags = "".join(
            ch for bit, ch in ((PF_R, "R"), (PF_W, "W"), (PF_X, "X"))
            if load["flags"] & bit
        )
        print(
            f"LOAD[{index}]: {flags or '-'} "
            f"0x{load['vaddr']:04x}..0x{load['mem_end']:04x} "
            f"align=0x{load['align']:x}"
        )
    if info["relocations"]:
        kinds = {}
        for _target, kind, _symbol, _addend in info["relocations"]:
            kinds[kind] = kinds.get(kind, 0) + 1
        print(
            "Relocations: "
            + ", ".join(f"type {kind}={count}" for kind, count in sorted(kinds.items()))
        )
    else:
        print("Relocations: -")
    print(f"SHA256: {sha256_hex(path)}")

def _selftest_partial_overlap_vaddr(rx_fields, rw_fields) -> int:
    rx_start = rx_fields[2]
    rx_end = rx_start + rx_fields[5]
    rw_align = max(1, rw_fields[7])
    overlap_vaddr = rx_start + (rx_fields[5] // 2)
    overlap_vaddr -= (overlap_vaddr - rw_fields[1]) % rw_align
    if overlap_vaddr < rx_start:
        overlap_vaddr += rw_align
    if overlap_vaddr >= rx_end:
        overlap_vaddr = rx_start
    return overlap_vaddr


def loader_selftest(args):
    source = rel(args.elf)
    original = source.read_bytes()
    validate_elf(source, dynamic_ok=True, library=args.library)

    def phdrs(blob: bytes):
        e_phoff = struct.unpack_from("<I", blob, 28)[0]
        e_phentsize, e_phnum = struct.unpack_from("<HH", blob, 42)
        for index in range(e_phnum):
            offset = e_phoff + index * e_phentsize
            yield index, offset, struct.unpack_from("<IIIIIIII", blob, offset)

    def relocation_file_offset(blob: bytes) -> int:
        loads = []
        dynamic = None
        for _index, _offset, fields in phdrs(blob):
            p_type, p_offset, p_vaddr, _paddr, p_filesz, _memsz, _flags, _align = fields
            if p_type == PT_LOAD:
                loads.append((p_vaddr, p_filesz, p_offset))
            elif p_type == PT_DYNAMIC:
                dynamic = (p_offset, p_filesz)
        if dynamic is None:
            raise RuntimeError("test ELF has no PT_DYNAMIC")
        tags = {}
        for offset in range(dynamic[0], dynamic[0] + dynamic[1], 8):
            tag, value = struct.unpack_from("<iI", blob, offset)
            if tag == DT_NULL:
                break
            tags[tag] = value
        address = tags.get(DT_RELA) or tags.get(DT_JMPREL)
        if address is None:
            return None
        for vaddr, filesz, file_offset in loads:
            if vaddr <= address and address + 12 <= vaddr + filesz:
                return file_offset + address - vaddr
        raise RuntimeError("test relocation table is not file-backed")

    mutations = {}
    blob = bytearray(original)
    blob[0] = 0
    mutations["bad-magic"] = blob

    blob = bytearray(original)
    for _index, offset, fields in phdrs(blob):
        if fields[0] == PT_LOAD:
            struct.pack_into("<I", blob, offset + 24, fields[6] | PF_W | PF_X)
            break
    mutations["rwx-segment"] = blob

    blob = bytearray(original)
    rx_fields = None
    rw_header_offset = None
    rw_fields = None
    for _index, offset, fields in phdrs(blob):
        if fields[0] != PT_LOAD:
            continue
        if fields[6] & PF_W:
            rw_header_offset = offset
            rw_fields = fields
        elif rx_fields is None:
            rx_fields = fields
    if rx_fields is None or rw_fields is None or rw_header_offset is None:
        raise SystemExit("selftest requires one RX and one RW PT_LOAD")
    # Place RW partway into RX while preserving ELF p_offset/p_vaddr
    # congruence. A fixed address (formerly 0x2000) stops being hostile when
    # a smaller valid app's RX segment ends below that address.
    overlap_vaddr = _selftest_partial_overlap_vaddr(rx_fields, rw_fields)
    struct.pack_into("<I", blob, rw_header_offset + 8, overlap_vaddr)
    mutations["overlap-near-text"] = blob

    blob = bytearray(original)
    for _index, offset, fields in phdrs(blob):
        if fields[0] == PT_LOAD and (fields[6] & PF_W):
            struct.pack_into("<I", blob, offset + 8, 0)
            break
    mutations["overlap"] = blob

    blob = bytearray(original)
    for _index, offset, fields in phdrs(blob):
        if fields[0] == PT_GNU_STACK:
            struct.pack_into("<I", blob, offset + 24, fields[6] | PF_X)
            break
    mutations["executable-stack"] = blob

    blob = bytearray(original)
    for _index, offset, fields in phdrs(blob):
        if fields[0] == PT_GNU_STACK:
            struct.pack_into("<I", blob, offset, PT_TLS)
            break
    mutations["tls-segment"] = blob

    blob = bytearray(original)
    relocation = relocation_file_offset(blob)
    if relocation is not None:
        info = struct.unpack_from("<I", blob, relocation + 4)[0]
        struct.pack_into("<I", blob, relocation + 4,
                         (info & ~0xFF) | R_RISCV_COPY)
        mutations["copy-relocation"] = blob

    with tempfile.TemporaryDirectory(prefix="protelf-selftest-") as temp:
        temp_dir = Path(temp)
        for name, mutated in mutations.items():
            path = temp_dir / f"{name}.elf"
            path.write_bytes(mutated)
            try:
                validate_elf(path, dynamic_ok=True, library=args.library)
            except SystemExit as exc:
                print(f"PASS {name}: {exc}")
            else:
                raise SystemExit(f"FAIL {name}: hostile ELF was accepted")
    print(f"PASS valid-split-dynamic: {source}")


PELF_BUNDLE_MAGIC   = 0x50454C42  # "PELB"
PELF_BUNDLE_VERSION_V1 = 1
PELF_BUNDLE_VERSION = 2
PELF_BUNDLE_VERSIONS_SUPPORTED = {
    PELF_BUNDLE_VERSION_V1, PELF_BUNDLE_VERSION,
}
# 32 bytes: magic, ver, flags, msize, esize, osize, table_size, payload_size,
# pad4. Still 32 bytes, and a legacy 3-section bundle has zeros where the two
# new words live -- so old bundles parse as table_size=0, payload_size=0 with
# no version branch anywhere.
PELF_BUNDLE_HDR_FMT = "<IHHIIIIIxxxx"
assert struct.calcsize(PELF_BUNDLE_HDR_FMT) == 32


def _fixed_ascii_field(data: bytes, offset: int, size: int, label: str,
                       *, allow_empty: bool = False) -> str:
    raw = data[offset:offset + size]
    if len(raw) != size or b"\0" not in raw:
        raise ValueError(f"manifest {label} is not NUL-terminated")
    value, padding = raw.split(b"\0", 1)
    if any(padding):
        raise ValueError(f"manifest {label} has non-zero terminator padding")
    if not value and not allow_empty:
        raise ValueError(f"manifest {label} is empty")
    if any(byte < 0x20 or byte > 0x7E for byte in value):
        raise ValueError(f"manifest {label} is not printable ASCII")
    return value.decode("ascii")


def _validate_resource_binding(manifest_data: bytes, resource_table: bytes,
                               resource_payload: bytes) -> None:
    """Correlate the manifest's resource fields with the actual sections.

    reserved[32] holds sha256(table) and reserved_v2[0:16] holds
    (count, table_size, payload_size, 0). Binding the table hash into the
    signed manifest is what makes the release signature cover the payload
    transitively: manifest -> table hash -> per-chunk stored-byte hashes ->
    payload bytes. A bundle with no resources leaves both regions zero.
    """
    declared = struct.unpack_from("<4I", manifest_data,
                                  MANIFEST_OFF_RESOURCE_META)
    count, table_size, payload_size, tail = declared
    table_hash = manifest_data[MANIFEST_OFF_RESOURCE_HASH:
                               MANIFEST_OFF_RESOURCE_HASH + 32]
    if tail != 0 or any(manifest_data[MANIFEST_OFF_RESOURCE_META + 16:356]):
        raise ValueError("manifest reserved_v2 tail bytes are non-zero")
    if not resource_table and not resource_payload:
        if count or table_size or payload_size or any(table_hash):
            raise ValueError(
                "manifest declares resources but the bundle carries none"
            )
        return
    if count == 0 or count > protelf_resources.RESOURCE_MAX_COUNT:
        raise ValueError(f"manifest resource count {count} is out of range")
    if table_size != len(resource_table):
        raise ValueError("manifest resource table size does not match the section")
    if payload_size != len(resource_payload):
        raise ValueError("manifest resource payload size does not match the section")
    if table_hash != hashlib.sha256(resource_table).digest():
        raise ValueError("manifest resource table hash mismatch")
    rows = protelf_resources.validate_resources(resource_table, resource_payload)
    if len(rows) != count:
        raise ValueError(
            f"manifest resource count {count} does not match the table's {len(rows)}"
        )


def validate_bundle_components(manifest_data: bytes, elf_data: bytes,
                               object_blob: bytes,
                               resource_table: bytes = b"",
                               resource_payload: bytes = b"",
                               *, release_signing: bool = False) -> list[bytes]:
    """Validate the bundle sections and return sidecar hashes.

    The resource pair is optional and defaults to empty, so a legacy
    three-section ABI-4 bundle validates through the same path.
    """
    if len(manifest_data) != PROTO_ELF_MANIFEST_SIZE:
        raise ValueError(
            f"manifest must be exactly {PROTO_ELF_MANIFEST_SIZE} bytes"
        )
    fields = struct.unpack_from("<25I", manifest_data, 0)
    if (
        fields[0] != PROTO_ELF_MANIFEST_MAGIC
        or fields[1] not in PROTO_ELF_MANIFEST_VERSIONS_SUPPORTED
        or fields[2] != PROTO_ELF_MANIFEST_SIZE
    ):
        raise ValueError("manifest magic, version, or header size is invalid")
    if fields[5] not in PROTO_ELF_ABI_VERSIONS_SUPPORTED:
        raise ValueError("manifest loader ABI does not match this tool")
    if fields[23] != len(elf_data):
        raise ValueError("manifest ELF size does not match the ELF section")
    if manifest_data[100:132] != hashlib.sha256(elf_data).digest():
        raise ValueError("manifest ELF hash does not match the ELF section")
    _fixed_ascii_field(manifest_data, 132, 32, "cli_name")
    _fixed_ascii_field(manifest_data, 164, 32, "display_name", allow_empty=True)
    _fixed_ascii_field(manifest_data, 196, 32, "version_name", allow_empty=True)
    _fixed_ascii_field(manifest_data, 260, 48, "package_name")
    _validate_resource_binding(manifest_data, resource_table, resource_payload)
    dynamic_flags = struct.unpack_from("<I", manifest_data, 396)[0]
    if (not dynamic_flags & PROTO_ELF_LAYOUT_SPLIT_DYNAMIC
            or dynamic_flags & ~PROTO_ELF_LAYOUT_FLAGS_SUPPORTED):
        raise ValueError("manifest dynamic layout is unsupported")
    if dynamic_flags & PROTO_ELF_LAYOUT_FLASH_XIP and fields[5] != PROTO_ELF_ABI_VERSION:
        raise ValueError(
            f"flash-XIP layout requires loader ABI {PROTO_ELF_ABI_VERSION}"
        )
    # Offset 402 is slot_span_log2, carved from reserved_v3 the same way
    # listen_port (400) was. The kernel bounds it, so the tail check starts
    # after it rather than treating a declared span as corruption.
    slot_span_log2 = manifest_data[402]
    if slot_span_log2 > PROTELF_XIP_SPAN_LOG2_MAX:
        raise ValueError(
            f"manifest slot_span_log2 {slot_span_log2} exceeds "
            f"{PROTELF_XIP_SPAN_LOG2_MAX}"
        )
    if slot_span_log2 and not dynamic_flags & PROTO_ELF_LAYOUT_FLASH_XIP:
        raise ValueError("slot_span_log2 requires the flash-XIP layout")
    # Offset 403 is large_heap_log2, carved the same way.
    large_heap_log2 = manifest_data[403]
    if large_heap_log2 != 0 and not PROTO_ELF_LARGE_HEAP_LOG2_MIN <= large_heap_log2 <= PROTO_ELF_LARGE_HEAP_LOG2:
        raise ValueError(
            f"manifest large_heap_log2 {large_heap_log2} is not 0 or "
            f"{PROTO_ELF_LARGE_HEAP_LOG2_MIN}..{PROTO_ELF_LARGE_HEAP_LOG2}"
        )
    if any(manifest_data[404:420]):
        raise ValueError("manifest reserved_v3 tail bytes are non-zero")
    required_permissions, optional_permissions = struct.unpack_from(
        "<2I", manifest_data, 308
    )
    listen_port = struct.unpack_from("<H", manifest_data, 400)[0]
    if listen_port and not (
        (required_permissions | optional_permissions) & PERM_NETWORK_LISTEN
    ):
        raise ValueError(
            "manifest named listen port requires network.listen permission"
        )
    if 0 < listen_port < 1024:
        raise ValueError("manifest named listen port must be at least 1024")
    if release_signing and (
        fields[24] != PROTO_ELF_SIGNATURE_ECDSA_P256_SHA256
        or struct.unpack_from("<I", manifest_data, 328)[0] == 0
    ):
        raise ValueError(
            "release signing requires P-256 policy and a positive version code"
        )

    if len(object_blob) < PROTO_OBJECT_HEADER_SIZE:
        raise ValueError("objects.pobj is truncated")
    magic, version, header_size, entry_size, entry_count, flags = (
        struct.unpack_from("<6I", object_blob, 0)
    )
    if (
        magic != PROTO_OBJECT_MAGIC
        or version != PROTO_OBJECT_VERSION
        or header_size != PROTO_OBJECT_HEADER_SIZE
        or entry_size != PROTO_OBJECT_ENTRY_SIZE
        or entry_count + 1 > MAX_OBJECTS
        or flags != 0
        or any(object_blob[24:32])
        or len(object_blob) != header_size + entry_count * entry_size
    ):
        raise ValueError("objects.pobj has an invalid header or length")

    object_count, object_size = struct.unpack_from("<2I", manifest_data, 356)
    if object_count != entry_count + 1:
        raise ValueError("manifest object count does not match objects.pobj")
    if object_size != len(object_blob):
        raise ValueError("manifest object-table size does not match objects.pobj")
    if manifest_data[364:396] != hashlib.sha256(object_blob).digest():
        raise ValueError("manifest object-table hash does not match objects.pobj")

    known_flags = (
        PROTO_OBJECT_REQUIRED | PROTO_OBJECT_OPTIONAL |
        (1 << 3) | (1 << 4) | PROTO_OBJECT_GLOBAL
    )
    record_hashes: list[bytes] = []
    object_ids: set[int] = set()
    sonames: set[str] = set()
    file_names: set[str] = set()
    for index in range(entry_count):
        offset = header_size + index * entry_size
        object_id, record_flags, abi_version, trust, file_size, reserved = (
            struct.unpack_from("<6I", object_blob, offset)
        )
        requirement = record_flags & (
            PROTO_OBJECT_REQUIRED | PROTO_OBJECT_OPTIONAL
        )
        if (
            object_id == 0
            or record_flags & ~known_flags
            or requirement not in (PROTO_OBJECT_REQUIRED, PROTO_OBJECT_OPTIONAL)
            or abi_version != PROTO_ELF_ABI_VERSION
            or trust > 2
            or not 1 <= file_size <= MAX_ELF_BYTES
            or reserved != 0
        ):
            raise ValueError(f"objects.pobj record {index} is invalid")
        soname = _fixed_ascii_field(object_blob, offset + 56, 48, "soname")
        file_name = _fixed_ascii_field(
            object_blob, offset + 104, 48, "file_name"
        )
        allowed = lambda text: all(
            ch.isascii() and (ch.isalnum() or ch in "._-+") for ch in text
        ) and ".." not in text
        if not allowed(soname) or not allowed(file_name):
            raise ValueError(f"objects.pobj record {index} has an unsafe name")
        if (
            object_id in object_ids
            or soname in sonames
            or file_name in file_names
        ):
            raise ValueError("objects.pobj contains a duplicate object identity")
        object_ids.add(object_id)
        sonames.add(soname)
        file_names.add(file_name)
        record_hashes.append(object_blob[offset + 24:offset + 56])
    return record_hashes


def parse_pelf_sections(
    data: bytes,
) -> tuple[bytes, bytes, bytes, bytes, bytes]:
    """Parse a canonical .pelf and validate exact section correlations.

    Always returns five sections. A legacy three-section bundle has zeros in
    the header words that now carry the resource sizes, so it decodes as
    empty table/payload without a version branch.
    """
    header_size = struct.calcsize(PELF_BUNDLE_HDR_FMT)
    if len(data) < header_size:
        raise ValueError("bundle is truncated")
    (magic, version, flags, manifest_size, elf_size, objects_size,
     table_size, payload_size) = struct.unpack_from("<IHHIIIII", data, 0)
    if magic != PELF_BUNDLE_MAGIC:
        raise ValueError("bundle magic is invalid")
    if version not in PELF_BUNDLE_VERSIONS_SUPPORTED:
        raise ValueError(f"unsupported bundle version {version}")
    if flags != 0:
        raise ValueError(f"unsupported bundle flags 0x{flags:04x}")
    if any(data[28:header_size]):
        raise ValueError("bundle header reserved bytes are non-zero")
    # Check the declared total against the ceiling FIRST, on the header alone,
    # so an oversized bundle is rejected before anything tries to hash or
    # structurally parse megabytes of attacker-supplied section.
    declared = (header_size + manifest_size + elf_size + objects_size +
                table_size + payload_size)
    if declared > MAX_BUNDLE_BYTES or len(data) > MAX_BUNDLE_BYTES:
        raise ValueError(
            f"bundle exceeds the 4 MiB marketplace ceiling "
            f"({declared} bytes declared)"
        )
    if manifest_size != PROTO_ELF_MANIFEST_SIZE:
        raise ValueError("bundle manifest size is not canonical")
    # Deliberately NOT MAX_BUNDLE_BYTES: the executable still has to fit its
    # flash-XIP run however large the resource sections grow. This is the
    # backstop; the manifest's declared span is the exact bound and the kernel
    # applies it.
    if not 1 <= elf_size <= MAX_ELF_BYTES:
        raise ValueError("bundle ELF size is outside the loader limit")
    max_objects_size = (
        PROTO_OBJECT_HEADER_SIZE +
        (MAX_OBJECTS - 1) * PROTO_OBJECT_ENTRY_SIZE
    )
    if not PROTO_OBJECT_HEADER_SIZE <= objects_size <= max_objects_size:
        raise ValueError("bundle object-table size is outside the ABI limit")
    if (table_size == 0) != (payload_size == 0):
        raise ValueError("bundle resource table and payload must both be present")
    if version == PELF_BUNDLE_VERSION_V1 and table_size != 0:
        raise ValueError("bundle v1 cannot contain resource sections")
    if declared != len(data):
        raise ValueError("bundle section sizes do not exactly match the file")
    offset = header_size
    manifest_data = data[offset:offset + manifest_size]
    offset += manifest_size
    elf_data = data[offset:offset + elf_size]
    offset += elf_size
    object_blob = data[offset:offset + objects_size]
    offset += objects_size
    resource_table = data[offset:offset + table_size]
    offset += table_size
    resource_payload = data[offset:offset + payload_size]
    validate_bundle_components(manifest_data, elf_data, object_blob,
                               resource_table, resource_payload)
    return manifest_data, elf_data, object_blob, resource_table, resource_payload


def pack_bundle(args) -> Path:
    """Create a canonical .pelf bundle (manifest + main ELF + object table)."""
    manifest_path = rel(args.manifest)
    elf_path      = rel(args.elf)
    objects_arg = getattr(args, "objects", None)
    if objects_arg is None:
        raise SystemExit("pack: ABI 4 requires --objects, even with no sidecars")
    objects_path = rel(objects_arg)

    for p, label in [
        (manifest_path, "manifest"),
        (elf_path, "ELF"),
        (objects_path, "objects"),
    ]:
        if not p.exists():
            raise SystemExit(f"pack: {label} not found: {p}")

    manifest_data = manifest_path.read_bytes()
    elf_data      = elf_path.read_bytes()
    objects_data  = objects_path.read_bytes()
    table_arg   = getattr(args, "resource_table", None)
    payload_arg = getattr(args, "resource_payload", None)
    table_data   = rel(table_arg).read_bytes() if table_arg else b""
    payload_data = rel(payload_arg).read_bytes() if payload_arg else b""
    validate_elf(elf_path, dynamic_ok=True)
    try:
        validate_bundle_components(manifest_data, elf_data, objects_data,
                                   table_data, payload_data)
    except ValueError as exc:
        raise SystemExit(f"pack: {exc}") from exc

    out = rel(args.out) if getattr(args, "out", None) else elf_path.with_suffix(".pelf")

    header = struct.pack(
        PELF_BUNDLE_HDR_FMT,
        PELF_BUNDLE_MAGIC,
        PELF_BUNDLE_VERSION,
        0,
        len(manifest_data),
        len(elf_data),
        len(objects_data),
        len(table_data),
        len(payload_data),
    )
    bundle = (header + manifest_data + elf_data + objects_data +
              table_data + payload_data)
    if len(bundle) > MAX_BUNDLE_BYTES:
        raise SystemExit(
            f"pack: bundle is {len(bundle):,} B, over the 4 MiB ceiling"
        )
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(bundle)

    print(f"pack: {out}  ({len(bundle):,} bytes total)")
    print(f"  manifest {len(manifest_data):,} B  ELF {len(elf_data):,} B"
          + (f"  objects {len(objects_data):,} B" if objects_data else "")
          + (f"  resources {len(table_data):,}+{len(payload_data):,} B"
             if table_data else ""))
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description="Build, upload, and install ProtoELF apps.")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_build = sub.add_parser("build")
    add_sdk_profile_arg(p_build)
    p_build.add_argument("--app", required=True, type=Path)
    p_build.add_argument("--name", default=None)
    p_build.add_argument("--out-dir", type=Path, default=None)
    p_build.add_argument("--gcc", default=os.environ.get("PROTOELF_GCC", "riscv32-esp-elf-gcc"))
    p_build.add_argument("--opt", default=None,
                             help="optimization level for the app sources (Os default; an app can declare its own with `optimize=` metadata)")
    p_build.add_argument(
        "--abi", type=int, default=PROTO_ELF_ABI_VERSION,
        choices=sorted(PROTO_ELF_ABI_VERSIONS_SUPPORTED),
        help="loader ABI; selects the linker script")
    p_build.add_argument("--library", action="append", default=[],
                         help="link SONAME=PATH; prefix with ? for an optional object-table entry")

    p_build_lib = sub.add_parser("build-lib")
    p_build_lib.add_argument("--lib", required=True, type=Path)
    p_build_lib.add_argument("--soname", required=True)
    p_build_lib.add_argument("--out-dir", type=Path, default=None)
    p_build_lib.add_argument("--gcc", default=os.environ.get("PROTOELF_GCC", "riscv32-esp-elf-gcc"))
    p_build_lib.add_argument("--library", action="append", default=[],
                             help="link a required SONAME=PATH dependency")

    p_bundle = sub.add_parser("bundle")
    p_bundle.add_argument("--app-id", required=True, type=int)
    p_bundle.add_argument("--library", action="append", required=True,
                          help="SONAME=PATH; prefix with ? for an optional dlopen library")
    p_bundle.add_argument("--out", required=True, type=Path)

    p_inspect = sub.add_parser("inspect")
    p_inspect.add_argument("--elf", required=True, type=Path)
    p_inspect.add_argument("--dynamic", action="store_true")
    p_inspect.add_argument("--library", action="store_true")

    p_selftest = sub.add_parser("selftest")
    p_selftest.add_argument("--elf", required=True, type=Path)
    p_selftest.add_argument("--library", action="store_true")

    p_manifest = sub.add_parser("manifest")
    p_manifest.add_argument("--elf", required=True, type=Path)
    p_manifest.add_argument("--out", type=Path, default=None)
    p_manifest.add_argument("--objects", type=Path, default=None)
    add_manifest_args(p_manifest)

    p_sign = sub.add_parser("sign")
    p_sign.add_argument("--manifest", required=True, type=Path)
    p_sign.add_argument("--elf", required=True, type=Path)
    p_sign.add_argument("--key", required=True, type=Path)
    p_sign.add_argument("--out", type=Path, default=None)
    p_sign.add_argument("--objects", required=True, type=Path)
    p_sign.add_argument("--key-password", default=None)

    p_upload = sub.add_parser("upload")
    add_serial_args(p_upload)
    p_upload.add_argument("--file", required=True, type=Path)
    p_upload.add_argument("--remote", required=True)

    p_install = sub.add_parser("install-dev")
    add_serial_args(p_install)
    p_install.add_argument("--manifest", required=True)
    p_install.add_argument("--elf", required=True)
    p_install.add_argument("--objects", default=None)
    p_install.add_argument("--replace", action="store_true")

    p_install_signed = sub.add_parser("install-signed")
    add_serial_args(p_install_signed)
    p_install_signed.add_argument("--manifest", required=True)
    p_install_signed.add_argument("--elf", required=True)
    p_install_signed.add_argument("--sig", required=True)
    p_install_signed.add_argument("--objects", default=None)
    p_install_signed.add_argument("--replace", action="store_true")

    p_run = sub.add_parser("run")
    add_serial_args(p_run)
    p_run.add_argument("--name", required=True)

    p_build_bundle = sub.add_parser(
        "build-bundle",
        help="Build main ELF + manifest + object table into .pelf; library payloads remain sidecars")
    add_sdk_profile_arg(p_build_bundle)
    p_build_bundle.add_argument("--app", required=True, type=Path)
    p_build_bundle.add_argument("--out-dir", type=Path, default=None)
    p_build_bundle.add_argument("--out", type=Path, default=None, help="Output .pelf path")
    p_build_bundle.add_argument("--gcc", default=os.environ.get("PROTOELF_GCC", "riscv32-esp-elf-gcc"))
    p_build_bundle.add_argument("--opt", default=None,
                                    help="optimization level for the app sources (Os default; an app can declare its own with `optimize=` metadata)")
    p_build_bundle.add_argument(
        "--library", action="append", default=[],
        help="link SONAME=PATH; records libraries in the object table but does not embed their payloads")
    add_manifest_args(p_build_bundle)

    p_pack = sub.add_parser(
        "pack",
        help="Create a .pelf from manifest + main ELF + required object table (not library payloads)")
    p_pack.add_argument("--manifest", required=True, type=Path,
                        help="Binary manifest file (.pmanifest)")
    p_pack.add_argument("--elf", required=True, type=Path,
                        help="Compiled ELF application")
    p_pack.add_argument("--objects", required=True, type=Path,
                        help="Object table (.pobj); referenced library payloads are not embedded")
    p_pack.add_argument("--resource-table", type=Path, default=None,
                        help="ABI 5 resource table (.prt)")
    p_pack.add_argument("--resource-payload", type=Path, default=None,
                        help="ABI 5 resource payload (.bin), paired with the table")
    p_pack.add_argument("--out", type=Path, default=None,
                        help="Output .pelf path (default: <elf-stem>.pelf)")

    p_deploy = sub.add_parser("deploy")
    add_sdk_profile_arg(p_deploy)
    add_serial_args(p_deploy)
    p_deploy.add_argument("--app", required=True, type=Path)
    p_deploy.add_argument("--out-dir", type=Path, default=None)
    p_deploy.add_argument("--gcc", default=os.environ.get("PROTOELF_GCC", "riscv32-esp-elf-gcc"))
    p_deploy.add_argument("--opt", default=None,
                              help="optimization level for the app sources (Os default; an app can declare its own with `optimize=` metadata)")
    p_deploy.add_argument("--remote-elf", default=None)
    p_deploy.add_argument("--remote-manifest", default=None)
    p_deploy.add_argument("--remote-objects", default=None)
    p_deploy.add_argument("--sig", type=Path, default=None)
    p_deploy.add_argument("--remote-sig", default=None)
    p_deploy.add_argument("--run", action="store_true")
    p_deploy.add_argument("--library", action="append", default=[],
                          help="link and separately stage SONAME=PATH; prefix with ? for optional dlopen")
    p_deploy.add_argument("--upgrade", action="store_true",
                          help="deprecated: deploy replaces installed apps by default")
    p_deploy.add_argument("--no-upgrade", action="store_true",
                          help="do not pass --replace to the device installer")
    add_manifest_args(p_deploy)

    args = parser.parse_args()
    if args.cmd == "build":
        build_app(args)
    elif args.cmd == "build-lib":
        build_library(args)
    elif args.cmd == "bundle":
        bundle_objects(args)
    elif args.cmd == "inspect":
        inspect_elf(args)
    elif args.cmd == "selftest":
        loader_selftest(args)
    elif args.cmd == "manifest":
        manifest(args)
    elif args.cmd == "sign":
        sign_app(args)
    elif args.cmd == "upload":
        upload_file(args)
    elif args.cmd == "install-dev":
        replace = " --replace" if args.replace else ""
        objects = f" {args.objects}" if args.objects else ""
        serial_command(args, f"install-dev{replace} {args.manifest} {args.elf}{objects}", "install-dev:")
    elif args.cmd == "install-signed":
        replace = " --replace" if args.replace else ""
        objects = f" {args.objects}" if args.objects else ""
        serial_command(args, f"install-signed{replace} {args.manifest} {args.elf}{objects} {args.sig}", "install-signed:")
    elif args.cmd == "run":
        serial_command(args, f"run {args.name}", "run")
    elif args.cmd == "build-bundle":
        build_bundle(args)
    elif args.cmd == "pack":
        pack_bundle(args)
    elif args.cmd == "deploy":
        deploy(args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
