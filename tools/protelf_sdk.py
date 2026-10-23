"""App-side source profiles and precompiled SDK library validation."""

import hashlib
import json
from pathlib import Path

SDK_ROOT = Path(__file__).resolve().parents[1]

CORE_SDK_SOURCES = (
    "loaded_apps/sdk/proto_loaded.c",
    "loaded_apps/sdk/proto_compiler_rt.c",
    "main/user/runtime/proto_gfx.c",
)
SDK_SOURCE_PROFILES = {
    "core": CORE_SDK_SOURCES,
    "full": CORE_SDK_SOURCES + (
        "main/user/runtime/proto_3d.c",
        "main/user/runtime/proto_3d_shape.c",
        "main/user/runtime/proto_3d_asset.c",
        "main/user/runtime/proto_3d_ray.c",
        "main/user/runtime/proto_3d_anim.c",
        "main/user/runtime/proto_3d_collide.c",
    ),
}


def sdk_sources(profile):
    try:
        sources = SDK_SOURCE_PROFILES[profile]
    except KeyError:
        raise ValueError(f"unknown SDK source profile: {profile!r}") from None
    if (SDK_ROOT / "lib").exists():
        sdk_libraries(profile)
        return ()
    return sources


def sdk_libraries(profile):
    if profile not in SDK_SOURCE_PROFILES:
        raise ValueError(f"unknown SDK source profile: {profile!r}")
    if not (SDK_ROOT / "lib").exists():
        return ()
    try:
        metadata = json.loads((SDK_ROOT / "lib/sdk-libraries.json").read_text(encoding="utf-8"))
        entry = metadata["profiles"][profile]
        expected = f"lib/libproto_{profile}.a"
        if metadata["format"] != 1 or entry["path"] != expected:
            raise ValueError("unexpected library manifest layout")
        archive = SDK_ROOT / expected
        if archive.is_symlink() or not archive.resolve().is_relative_to(SDK_ROOT.resolve()):
            raise ValueError("library escapes SDK")
        data = archive.read_bytes()
        if not data.startswith(b"!<arch>\n") or hashlib.sha256(data).hexdigest() != entry["sha256"]:
            raise ValueError("library checksum or archive format mismatch")
        return (archive,)
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise SystemExit(f"SDK profile {profile!r} library is incomplete or invalid: {error}") from None


def add_sdk_profile_arg(parser):
    parser.add_argument(
        "--sdk-profile", choices=tuple(SDK_SOURCE_PROFILES), default="full",
        help="app-side SDK sources: core omits Game3D helper implementations; "
             "full adds the Game3D app-side helper library")
