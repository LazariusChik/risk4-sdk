#!/usr/bin/env python3
"""Canonical ProtoELF v5 resource-table packer and validator."""

from __future__ import annotations

import hashlib
import struct
from pathlib import Path
from typing import Iterable


RESOURCE_MAGIC = 0x50525343  # "PRSC"
RESOURCE_VERSION = 1
RESOURCE_NAME_MAX = 48
RESOURCE_CHUNK_BYTES = 3840
RESOURCE_STORED_MAX = 3872
RESOURCE_MAX_COUNT = 64
# PROTO_RESOURCE_MAX_CHUNKS: the device keeps a running app's table resident.
RESOURCE_MAX_CHUNKS = 4096
RESOURCE_CODEC_RAW = 0
RESOURCE_CODEC_LZ4 = 1
RESOURCE_HEADER_FMT = "<8I"
RESOURCE_RECORD_FMT = "<48s4I32s"
RESOURCE_CHUNK_FMT = "<4I32s"
RESOURCE_HEADER_SIZE = struct.calcsize(RESOURCE_HEADER_FMT)
RESOURCE_RECORD_SIZE = struct.calcsize(RESOURCE_RECORD_FMT)
RESOURCE_CHUNK_SIZE = struct.calcsize(RESOURCE_CHUNK_FMT)


def _clean_name(name: str) -> bool:
    return (
        0 < len(name.encode("ascii", errors="ignore")) < RESOURCE_NAME_MAX
        and name.isascii()
        and ".." not in name
        and not name.startswith("/")
        and not name.endswith("/")
        and "//" not in name
        and all(ch.isalnum() or ch in "._-/" for ch in name)
    )


def _fixed_name(name: str) -> bytes:
    if not _clean_name(name):
        raise ValueError(f"invalid resource name {name!r}")
    raw = name.encode("ascii")
    return raw + bytes(RESOURCE_NAME_MAX - len(raw))


def _decode_name(raw: bytes) -> str:
    if len(raw) != RESOURCE_NAME_MAX or b"\0" not in raw:
        raise ValueError("resource name is not NUL-terminated")
    value, padding = raw.split(b"\0", 1)
    if any(padding):
        raise ValueError("resource name has non-zero padding")
    try:
        name = value.decode("ascii")
    except UnicodeDecodeError as exc:
        raise ValueError("resource name is not ASCII") from exc
    if not _clean_name(name):
        raise ValueError(f"invalid resource name {name!r}")
    return name


def _emit_length(out: bytearray, extra: int) -> None:
    while extra >= 255:
        out.append(255)
        extra -= 255
    out.append(extra)


def lz4_compress_block(data: bytes) -> bytes:
    """Small deterministic raw-block LZ4 compressor used by the packer."""
    if not data:
        return b""
    out = bytearray()
    table: dict[bytes, int] = {}
    anchor = 0
    i = 0
    size = len(data)
    while i + 4 <= size:
        key = data[i:i + 4]
        reference = table.get(key)
        table[key] = i
        if reference is None or i - reference > 0xFFFF:
            i += 1
            continue
        match_len = 4
        while i + match_len < size and data[reference + match_len] == data[i + match_len]:
            match_len += 1
        literal_len = i - anchor
        token_pos = len(out)
        out.append(0)
        token = min(literal_len, 15) << 4
        if literal_len >= 15:
            _emit_length(out, literal_len - 15)
        out.extend(data[anchor:i])
        offset = i - reference
        out.extend((offset & 0xFF, offset >> 8))
        encoded_match = match_len - 4
        token |= min(encoded_match, 15)
        if encoded_match >= 15:
            _emit_length(out, encoded_match - 15)
        out[token_pos] = token
        match_end = i + match_len
        update = i + 1
        while update + 4 <= match_end:
            table[data[update:update + 4]] = update
            update += 1
        i = match_end
        anchor = i
    if anchor < size or not out:
        literal_len = size - anchor
        token_pos = len(out)
        out.append(min(literal_len, 15) << 4)
        if literal_len >= 15:
            _emit_length(out, literal_len - 15)
        out.extend(data[anchor:])
        # token_pos is retained to make the final-literal sequence explicit.
        assert out[token_pos] >> 4 == min(literal_len, 15)
    return bytes(out)


def lz4_decompress_block(stored: bytes, expected_size: int) -> bytes:
    src = memoryview(stored)
    out = bytearray()
    offset = 0
    while offset < len(src):
        token = src[offset]
        offset += 1
        literal_len = token >> 4
        if literal_len == 15:
            while True:
                if offset >= len(src):
                    raise ValueError("truncated LZ4 literal length")
                extension = src[offset]
                offset += 1
                literal_len += extension
                if extension != 255:
                    break
        if literal_len > len(src) - offset or literal_len > expected_size - len(out):
            raise ValueError("LZ4 literals exceed chunk bounds")
        out.extend(src[offset:offset + literal_len])
        offset += literal_len
        if offset == len(src):
            break
        if len(src) - offset < 2:
            raise ValueError("truncated LZ4 match offset")
        match_offset = int(src[offset]) | (int(src[offset + 1]) << 8)
        offset += 2
        if match_offset == 0 or match_offset > len(out):
            raise ValueError("invalid LZ4 match offset")
        match_len = (token & 0x0F) + 4
        if (token & 0x0F) == 15:
            while True:
                if offset >= len(src):
                    raise ValueError("truncated LZ4 match length")
                extension = src[offset]
                offset += 1
                match_len += extension
                if extension != 255:
                    break
        if match_len > expected_size - len(out):
            raise ValueError("LZ4 match exceeds chunk bounds")
        for _ in range(match_len):
            out.append(out[-match_offset])
    if len(out) != expected_size:
        raise ValueError("LZ4 output size does not match chunk record")
    return bytes(out)


def parse_resource_specs(values: Iterable[str]) -> list[tuple[str, bytes]]:
    resources: list[tuple[str, bytes]] = []
    seen: set[str] = set()
    for value in values:
        if "=" not in value:
            raise ValueError("resource must be NAME=PATH")
        name, path_text = value.split("=", 1)
        if not _clean_name(name) or name in seen:
            raise ValueError(f"invalid or duplicate resource name {name!r}")
        path = Path(path_text)
        if not path.is_absolute():
            path = Path.cwd() / path
        try:
            payload = path.read_bytes()
        except OSError as exc:
            raise ValueError(f"cannot read resource {name!r}: {path}") from exc
        if not payload:
            raise ValueError(f"resource {name!r} is empty")
        seen.add(name)
        resources.append((name, payload))
    if len(resources) > RESOURCE_MAX_COUNT:
        raise ValueError(f"at most {RESOURCE_MAX_COUNT} resources are supported")
    return resources


def pack_resources(resources: Iterable[tuple[str, bytes]]) -> tuple[bytes, bytes]:
    resources = list(resources)
    if len(resources) > RESOURCE_MAX_COUNT:
        raise ValueError(f"at most {RESOURCE_MAX_COUNT} resources are supported")
    records: list[bytes] = []
    chunks: list[bytes] = []
    payload = bytearray()
    seen: set[str] = set()
    for name, content in resources:
        if name in seen:
            raise ValueError(f"duplicate resource name {name!r}")
        name_field = _fixed_name(name)
        if not content:
            raise ValueError(f"resource {name!r} is empty")
        first_chunk = len(chunks)
        for start in range(0, len(content), RESOURCE_CHUNK_BYTES):
            raw = content[start:start + RESOURCE_CHUNK_BYTES]
            compressed = lz4_compress_block(raw)
            codec = RESOURCE_CODEC_LZ4 if len(compressed) < len(raw) else RESOURCE_CODEC_RAW
            stored = compressed if codec == RESOURCE_CODEC_LZ4 else raw
            if not stored or len(stored) > RESOURCE_STORED_MAX or len(stored) + len(raw) > 8192:
                raise ValueError(f"resource {name!r} produced an invalid chunk")
            chunks.append(struct.pack(
                RESOURCE_CHUNK_FMT, len(payload), len(stored), len(raw), codec,
                hashlib.sha256(stored).digest(),
            ))
            payload.extend(stored)
        records.append(struct.pack(
            RESOURCE_RECORD_FMT, name_field, len(content), first_chunk,
            len(chunks) - first_chunk, 0, hashlib.sha256(content).digest(),
        ))
        seen.add(name)
    table = struct.pack(
        RESOURCE_HEADER_FMT, RESOURCE_MAGIC, RESOURCE_VERSION,
        RESOURCE_HEADER_SIZE, RESOURCE_RECORD_SIZE, len(records),
        RESOURCE_CHUNK_SIZE, len(chunks), len(payload),
    ) + b"".join(records) + b"".join(chunks)
    validate_resources(table, bytes(payload))
    return table, bytes(payload)


def validate_resources(table: bytes, payload: bytes) -> list[dict[str, object]]:
    if not table:
        if payload:
            raise ValueError("resource payload exists without a table")
        return []
    if len(table) < RESOURCE_HEADER_SIZE:
        raise ValueError("resource table is truncated")
    fields = struct.unpack_from(RESOURCE_HEADER_FMT, table, 0)
    magic, version, header_size, record_size, count, chunk_size, chunk_count, payload_size = fields
    expected_table = header_size + count * record_size + chunk_count * chunk_size
    if (
        magic != RESOURCE_MAGIC or version != RESOURCE_VERSION
        or header_size != RESOURCE_HEADER_SIZE
        or record_size != RESOURCE_RECORD_SIZE
        or chunk_size != RESOURCE_CHUNK_SIZE
        or count == 0 or count > RESOURCE_MAX_COUNT
        or chunk_count == 0 or chunk_count > RESOURCE_MAX_CHUNKS
        or expected_table != len(table) or payload_size != len(payload)
    ):
        raise ValueError("resource table header or size correlation is invalid")
    result: list[dict[str, object]] = []
    names: set[str] = set()
    expected_first = 0
    expected_payload_offset = 0
    for index in range(count):
        offset = header_size + index * record_size
        name_raw, total_size, first_chunk, record_chunks, flags, digest = struct.unpack_from(
            RESOURCE_RECORD_FMT, table, offset
        )
        name = _decode_name(name_raw)
        if name in names or flags != 0 or total_size == 0 or record_chunks == 0 or first_chunk != expected_first:
            raise ValueError(f"resource record {index} is invalid")
        names.add(name)
        decoded = bytearray()
        raw_total = 0
        for chunk_index in range(first_chunk, first_chunk + record_chunks):
            if chunk_index >= chunk_count:
                raise ValueError(f"resource record {index} exceeds the chunk table")
            chunk_offset = header_size + count * record_size + chunk_index * chunk_size
            payload_offset, stored_size, raw_size, codec, stored_hash = struct.unpack_from(
                RESOURCE_CHUNK_FMT, table, chunk_offset
            )
            if (
                payload_offset != expected_payload_offset or stored_size == 0
                or stored_size > RESOURCE_STORED_MAX or raw_size == 0
                or raw_size > RESOURCE_CHUNK_BYTES
                or (chunk_index + 1 < first_chunk + record_chunks
                    and raw_size != RESOURCE_CHUNK_BYTES)
                or stored_size + raw_size > 8192
                or payload_offset + stored_size > len(payload)
                or codec not in (RESOURCE_CODEC_RAW, RESOURCE_CODEC_LZ4)
            ):
                raise ValueError(f"resource chunk {chunk_index} is invalid")
            stored = payload[payload_offset:payload_offset + stored_size]
            if hashlib.sha256(stored).digest() != stored_hash:
                raise ValueError(f"resource chunk {chunk_index} hash mismatch")
            if codec == RESOURCE_CODEC_RAW:
                if stored_size != raw_size:
                    raise ValueError(f"raw resource chunk {chunk_index} has mismatched sizes")
                raw = stored
            else:
                raw = lz4_decompress_block(stored, raw_size)
                if stored_size >= raw_size:
                    raise ValueError(f"LZ4 resource chunk {chunk_index} is not smaller")
            decoded.extend(raw)
            raw_total += raw_size
            expected_payload_offset += stored_size
        if (
            raw_total != total_size or len(decoded) != total_size
            or hashlib.sha256(decoded).digest() != digest
        ):
            raise ValueError(f"resource record {index} content correlation failed")
        expected_first += record_chunks
        result.append({"name": name, "size": total_size, "chunks": record_chunks})
    if expected_first != chunk_count or expected_payload_offset != len(payload):
        raise ValueError("resource chunk or payload table has trailing data")
    return result
