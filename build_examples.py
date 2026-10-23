#!/usr/bin/env python3
"""Build, validate and package SDK examples without executing device probes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "tools"))
import protelf_dev


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gcc", default=os.environ.get("PROTOELF_GCC", "riscv32-esp-elf-gcc"))
    args = parser.parse_args()
    gcc = protelf_dev.find_gcc(args.gcc)
    compiler = subprocess.check_output([gcc, "--version"], text=True).splitlines()[0]
    results = {}
    for name, app_id, heap, profile in (
            ("hello", 7001, 4096, "core"),
            ("hello2d", 7002, 16384, "core"),
            ("hello3d", 7003, 65536, "full"),
            ("pentest", 7004, 4096, "core")):
        # Keep the installed demo distinct from firmware's internal pentest app.
        app_name = "pentest-demo" if name == "pentest" else name
        subprocess.run([
            sys.executable, str(ROOT / "tools/protelf_dev.py"), "build-bundle",
            "--sdk-profile", profile, "--gcc", gcc,
            "--app", f"examples/{name}", "--app-id", str(app_id),
            "--name", app_name, "--package", f"dev.proto.sdk.{name}",
            "--heap", str(heap), "--out", f"build/{name}.pelf",
        ], cwd=ROOT, check=True)
        elf = ROOT / f"build/protelf_{app_name}/app.elf"
        subprocess.run([
            sys.executable, str(ROOT / "tools/protelf_dev.py"),
            "selftest", "--elf", str(elf),
        ], cwd=ROOT, check=True)
        bundle = ROOT / f"build/{name}.pelf"
        results[name] = {"app_name": app_name, "sdk_profile": profile, "bytes": bundle.stat().st_size,
                         "sha256": hashlib.sha256(bundle.read_bytes()).hexdigest()}
    report = {"compiler": compiler, "python": sys.version.split()[0],
              "target": "rv32imafc / ilp32f", "sdk_profiles": ["core", "full"],
              "validation": "build, ELF hostile-input selftest, package validation; no device run",
              "bundles": results}
    (ROOT / "build/validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("PASS standalone SDK: " + ", ".join(f"build/{name}.pelf" for name in results))
    print("Validation details: build/validation.json. Device execution remains unverified.")


if __name__ == "__main__":
    main()
