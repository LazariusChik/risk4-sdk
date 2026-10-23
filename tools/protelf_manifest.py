#!/usr/bin/env python3
import argparse
import hashlib
import re
import struct
import sys
from pathlib import Path

try:
    from . import protelf_resources
except ImportError:  # pragma: no cover - direct `python tools/protelf_manifest.py`
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import protelf_resources


PROTO_ELF_MANIFEST_MAGIC = 0x50454C46
PROTO_ELF_MANIFEST_VERSION = 5
PROTO_ELF_ABI_VERSION = 5
PROTO_ELF_LAYOUT_SPLIT_DYNAMIC = 1 << 0
PROTO_ELF_LAYOUT_FLASH_XIP = 1 << 1
PROTO_ELF_XIP_SLOT_COUNT = 31
PROTO_ELF_XIP_SPAN_LOG2_MAX = PROTO_ELF_XIP_SLOT_COUNT.bit_length() - 1
# The kernel's one boot-reserved large-heap pool (proto_abi.h).
PROTO_ELF_LARGE_HEAP_LOG2_MIN = 16
PROTO_ELF_LARGE_HEAP_LOG2 = 23
PROTO_ELF_LARGE_HEAP_BYTES = 1 << PROTO_ELF_LARGE_HEAP_LOG2
SECURITY_PROFILE_NORMAL = 2
DEFAULT_MAILBOX_DEPTH = 8
KERNEL_APP_FLAG_INSTALLED = 1 << 5
KERNEL_APP_FLAG_UNSIGNED_DEV = 1 << 6
KERNEL_APP_FLAG_EXIT_HOOK = 1 << 9
KERNEL_APP_FLAG_EXIT_CONFIRM = 1 << 10
PROTO_APP_INSTALL_INSTALLED = 5
PROTO_APP_PERMISSION_ALL = (1 << 9) - 1
PROTO_ELF_MANIFEST_SIZE = 420
PROTO_ELF_OBJECT_TABLE_MAGIC = 0x504F424A
PROTO_ELF_OBJECT_TABLE_VERSION = 1
PROTO_ELF_OBJECT_HEADER_SIZE = 32
PROTO_ELF_OBJECT_ENTRY_SIZE = 152
PROTO_ELF_MAX_OBJECTS = 8
PROTOELF_DEFAULT_QUOTA_BYTES = 16 * 1024
PROTO_ELF_SIGNATURE_ECDSA_P256_SHA256 = 1


def current_kernel_abi_version() -> int:
    """Keep generated manifests locked to the SDK/kernel wire contract."""
    abi_header = (
        Path(__file__).resolve().parents[1] / "main" / "include" / "proto_abi.h"
    )
    match = re.search(
        r"^\s*#define\s+PROTO_ABI_VERSION\s+(\d+)u?\s*$",
        abi_header.read_text(encoding="utf-8"),
        flags=re.MULTILINE,
    )
    if match is None:
        raise RuntimeError(f"cannot read PROTO_ABI_VERSION from {abi_header}")
    return int(match.group(1))


PROTO_ABI_VERSION = current_kernel_abi_version()


def normalize_package(text: str) -> str:
    out = []
    prev_dot = False
    for ch in text.lower():
        if "a" <= ch <= "z" or "0" <= ch <= "9" or ch == "-":
            out.append(ch)
            prev_dot = False
        elif ch in "._":
            if out and not prev_dot:
                out.append(".")
                prev_dot = True
    package = "".join(out).strip(".")
    labels = [label for label in package.split(".") if label]
    package = ".".join(labels)
    if not package or len(package) >= 48 or ".." in package or package.isdigit():
        raise SystemExit(f"invalid package name after normalization: {text!r}")
    return package


def validate_package(package: str) -> str:
    if not package or len(package) >= 48:
        raise SystemExit("package must be 1..47 characters")
    labels = package.split(".")
    if any(not label for label in labels):
        raise SystemExit("package labels cannot be empty")
    for ch in package:
        if not ("a" <= ch <= "z" or "0" <= ch <= "9" or ch in ".-"):
            raise SystemExit("package may contain only lowercase letters, digits, dots, and hyphens")
    # INV-051 (main/include/kernel_package_name.h): /apps/<digits> belongs to
    # built-in and id-installed apps, so an all-digit package would own one.
    if package.isdigit():
        raise SystemExit("package cannot be all digits; it would collide with a built-in app id")
    return package


def fixed_text(text: str, size: int) -> bytes:
    raw = text.encode("ascii")
    if not raw or len(raw) >= size:
        raise SystemExit(f"text field must be 1..{size - 1} ASCII bytes: {text!r}")
    return raw + b"\0" * (size - len(raw))


def main() -> int:
    parser = argparse.ArgumentParser(description="Create a ProtoELF binary manifest.")
    parser.add_argument("--elf", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
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
        help="monotonic release identity embedded in and covered by the release signature",
    )
    parser.add_argument("--stack", type=int, default=4096)
    parser.add_argument("--quota", type=int, default=PROTOELF_DEFAULT_QUOTA_BYTES)
    parser.add_argument("--heap", type=int, default=4096)
    parser.add_argument("--grant-caps", type=lambda s: int(s, 0), default=0)
    parser.add_argument("--deny-caps", type=lambda s: int(s, 0), default=0)
    parser.add_argument("--app-flags", type=lambda s: int(s, 0), default=0,
                        help="additional non-privileged KernelApp flags")
    parser.add_argument("--ipc-targets", type=lambda s: int(s, 0), default=0)
    parser.add_argument("--required-permissions", type=lambda s: int(s, 0), default=0)
    parser.add_argument("--optional-permissions", type=lambda s: int(s, 0), default=0)
    parser.add_argument("--granted-permissions", type=lambda s: int(s, 0), default=0)
    parser.add_argument("--denied-permissions", type=lambda s: int(s, 0), default=0)
    parser.add_argument(
        "--slot-span-log2", type=int, default=0, choices=tuple(range(PROTO_ELF_XIP_SPAN_LOG2_MAX + 1)),
        help="flash-XIP arena span as a power-of-two slot count exponent: "
             "0=1 slot (128 KiB RX + 128 KiB RW), 1=2 slots, 2=4 slots, 3=8 slots, 4=16 slots")
    parser.add_argument("--listen-port", type=int, default=0,
                        help="declared inbound TCP port; 0 permits ephemeral only")
    parser.add_argument(
        "--large-heap-log2", type=int, default=0, choices=(0, *range(PROTO_ELF_LARGE_HEAP_LOG2_MIN, PROTO_ELF_LARGE_HEAP_LOG2 + 1)),
        help="request a private large-heap slice: 0=none, 16=64 KiB through 23=8 MiB; 22=4 MiB")
    parser.add_argument("--objects", type=Path, default=None,
                        help="objects.pobj for a manifest v4 dynamic bundle")
    parser.add_argument("--resource-table", type=Path, default=None,
                        help="ABI 5 resource table (.prt) to bind into the manifest")
    parser.add_argument("--resource-payload", type=Path, default=None,
                        help="ABI 5 resource payload (.bin) paired with the table")
    parser.add_argument("--flash-xip", action="store_true",
                        help="declare the ABI 5 execute-in-place flash layout")
    args = parser.parse_args()

    if not 1 <= args.version_code <= 0xFFFFFFFF:
        raise SystemExit("version-code must be in 1..4294967295")
    if not 1 <= args.app_id <= 999999:
        raise SystemExit("app-id must be in 1..999999")
    if not 0 <= args.listen_port <= 65535:
        raise SystemExit("listen-port must be in 0..65535")
    if args.slot_span_log2 != 0 and not args.flash_xip:
        # The span is a run of flash-XIP arena slots; it means nothing for a
        # RAM-loaded image, and the kernel rejects the combination outright.
        raise SystemExit("slot-span-log2 requires a flash-XIP (ABI 5) image")
    effective_quota = args.quota or 4096
    effective_heap = args.heap or effective_quota
    large_heap = (1 << args.large_heap_log2) if args.large_heap_log2 else 0
    if args.flash_xip and effective_quota > (128 * 1024 << args.slot_span_log2) + large_heap:
        raise SystemExit("heap quota exceeds the declared flash-XIP span plus large heap")
    if effective_heap > effective_quota:
        raise SystemExit("initial heap exceeds the manifest memory quota")
    if args.listen_port != 0 and args.listen_port < 1024:
        raise SystemExit("named listen ports below 1024 are kernel-reserved")
    if args.listen_port != 0 and not (
        (args.required_permissions | args.optional_permissions) & (1 << 7)
    ):
        raise SystemExit("listen-port requires network.listen permission")

    permission_values = (
        args.required_permissions,
        args.optional_permissions,
        args.granted_permissions,
        args.denied_permissions,
    )
    if any(value & ~PROTO_APP_PERMISSION_ALL for value in permission_values):
        raise SystemExit("permission masks contain unknown bits")
    if args.required_permissions & args.optional_permissions:
        raise SystemExit("a permission cannot be both required and optional")
    declared_permissions = args.required_permissions | args.optional_permissions
    if args.granted_permissions & ~declared_permissions:
        raise SystemExit("initial grants must be declared required or optional permissions")
    if args.denied_permissions & ~declared_permissions:
        raise SystemExit("initial denials must be declared required or optional permissions")
    if args.granted_permissions & args.denied_permissions:
        raise SystemExit("a permission cannot be both granted and denied")

    elf = args.elf.read_bytes()
    digest = hashlib.sha256(elf).digest()
    # Resource binding. The table's SHA-256 goes into the signed manifest, so
    # the release signature covers the payload transitively: manifest ->
    # table hash -> per-chunk stored-byte hashes -> payload bytes.
    if (args.resource_table is None) != (args.resource_payload is None):
        raise SystemExit("--resource-table and --resource-payload go together")
    resource_table = (args.resource_table.read_bytes()
                      if args.resource_table else b"")
    resource_payload_size = (args.resource_payload.stat().st_size
                             if args.resource_payload else 0)
    if resource_table:
        resource_rows = protelf_resources.validate_resources(
            resource_table, args.resource_payload.read_bytes()
        )
        resource_count = len(resource_rows)
        resource_hash = hashlib.sha256(resource_table).digest()
    else:
        resource_count = 0
        resource_hash = b"\0" * 32
    dynamic_flags = PROTO_ELF_LAYOUT_SPLIT_DYNAMIC
    if args.flash_xip:
        dynamic_flags |= PROTO_ELF_LAYOUT_FLASH_XIP
    display = args.display or args.name
    package = validate_package(args.package or f"dev.proto.{normalize_package(args.name)}")
    if args.required_permissions & args.denied_permissions:
        raise SystemExit("required permissions cannot be initially denied")
    if args.objects is None:
        raise SystemExit("ProtoELF ABI 4 requires --objects, even for single-object apps")
    object_table = args.objects.read_bytes()
    if len(object_table) < PROTO_ELF_OBJECT_HEADER_SIZE:
        raise SystemExit("objects.pobj is truncated")
    magic, version, header_size, entry_size, entry_count, flags = struct.unpack_from(
        "<6I", object_table, 0
    )
    if (
        magic != PROTO_ELF_OBJECT_TABLE_MAGIC
        or version != PROTO_ELF_OBJECT_TABLE_VERSION
        or header_size != PROTO_ELF_OBJECT_HEADER_SIZE
        or entry_size != PROTO_ELF_OBJECT_ENTRY_SIZE
        or entry_count + 1 > PROTO_ELF_MAX_OBJECTS
        or flags != 0
        or any(object_table[24:32])
        or len(object_table) != header_size + entry_count * entry_size
    ):
        raise SystemExit("invalid ProtoELF object table")
    object_count = entry_count + 1
    object_table_hash = hashlib.sha256(object_table).digest()
    manifest_version = PROTO_ELF_MANIFEST_VERSION
    manifest_size = PROTO_ELF_MANIFEST_SIZE
    elf_abi = PROTO_ELF_ABI_VERSION

    values = [
        PROTO_ELF_MANIFEST_MAGIC,
        manifest_version,
        manifest_size,
        args.app_id,
        args.pid,
        elf_abi,
        PROTO_ABI_VERSION,
        SECURITY_PROFILE_NORMAL,
        0,
        args.grant_caps,
        args.deny_caps,
        # Loaded-app storage authority is derived from granted permissions at
        # launch. A raw storage policy would bypass the marketplace consent
        # channel (and SYSTEM storage would expose /etc and other apps).
        0,
        args.app_flags | KERNEL_APP_FLAG_INSTALLED | KERNEL_APP_FLAG_UNSIGNED_DEV,
        args.stack,
        1,
        0xFFFFFFFF,
        DEFAULT_MAILBOX_DEPTH,
        args.quota,
        args.heap,
        0,
        1,
        0,
        args.ipc_targets,
        len(elf),
        PROTO_ELF_SIGNATURE_ECDSA_P256_SHA256,
    ]
    blob = struct.pack("<25I", *values)
    blob += digest
    blob += fixed_text(args.name, 32)
    blob += fixed_text(display, 32)
    blob += fixed_text(args.version, 32)
    # reserved[32] -> resource_table_sha256 (ABI 5); zero when the app ships
    # no resources, which keeps V4 bundles byte-identical.
    blob += resource_hash
    blob += fixed_text(package, 48)
    blob += struct.pack(
        "<5I",
        args.required_permissions,
        args.optional_permissions,
        args.granted_permissions,
        args.denied_permissions,
        PROTO_APP_INSTALL_INSTALLED,
    )
    # The numeric release identity is part of the manifest rather than
    # marketplace form metadata. Release signatures cover the complete raw
    # manifest, so a catalog signer cannot relabel an old bundle as a newer
    # version and poison the device's rollback ledger.
    blob += struct.pack("<I", args.version_code)
    # reserved_v2[0:16] -> (resource_count, table_size, payload_size, 0);
    # the remaining 8 bytes stay reserved and must be zero.
    blob += struct.pack("<4I", resource_count, len(resource_table),
                        resource_payload_size, 0)
    blob += b"\0" * 8
    blob += struct.pack("<2I", object_count, len(object_table))
    blob += object_table_hash
    blob += struct.pack("<I", dynamic_flags)
    blob += struct.pack("<H", args.listen_port)
    blob += struct.pack("<B", args.slot_span_log2)
    blob += struct.pack("<B", args.large_heap_log2)
    blob += b"\0" * 16
    if len(blob) != manifest_size:
        raise SystemExit(f"manifest packing bug: {len(blob)} bytes")
    args.out.write_bytes(blob)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
