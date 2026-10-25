"""Preflight and build the development probe only. Never builds gameplay."""
import json
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import pefile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
OUT = ROOT / "build/dev_probe"
SHEET = HERE / "probe.sheet.json"

def preflight(sheet):
    errors = []
    for tab in ("natives", "settings"):
        columns = sheet.get(tab, {}).get("columns", [])
        rows = sheet.get(tab, {}).get("rows", [])
        ids = set()
        if not columns or not rows:
            errors.append(f"{tab}: missing columns/rows")
        for row in rows:
            rowid = row.get("id", "<missing>")
            if rowid in ids:
                errors.append(f"{tab}:{rowid}: duplicate id")
            ids.add(rowid)
            for col in columns:
                value = row.get(col)
                if value is None or value == "" or value in ("unresolved", "pending"):
                    errors.append(f"{tab}:{rowid}:{col}: unfilled/unverified")
            if row.get("status") != "verified-inventory":
                errors.append(f"{tab}:{rowid}: inventory status not verified")
    ns = sheet["natives"]["rows"]
    for row in ns:
        try:
            assert 0 < int(row["hash"], 16) < 2**64
            if row["expectedRva"] != "capture-only":
                assert 0 < int(row["expectedRva"], 16) < 2**32
        except (AssertionError, ValueError, KeyError):
            errors.append(f"natives:{row.get('id')}: invalid hash/RVA")
    required = {"WAIT", "PLAYER_PED_ID"}
    if not required.issubset({row.get("id") for row in ns}):
        errors.append("natives: missing sentinel references")
    config = {row["id"]: row["value"] for row in sheet["settings"]["rows"]}
    for key in ("probe_id", "game_version", "game_timestamp", "game_image_size", "shv_sha256", "shv_timestamp", "shv_image_size", "last_native_rva", "register_export", "wait_export", "init_export", "warmup_frames", "required_launch_token", "distribution"):
        if key not in config:
            errors.append(f"settings:{key}: unresolved configuration reference")
    if config.get("distribution") != "development-diagnostic-only-never-package":
        errors.append("settings: development-only scope missing")
    return errors

def main():
    sheet = json.loads(SHEET.read_text(encoding="utf-8"))
    errors = preflight(sheet)
    OUT.mkdir(parents=True, exist_ok=True)
    report = {"scope": "development diagnostic only; gameplay preflight remains separate", "errors": errors,
              "nativeRows": len(sheet["natives"]["rows"]), "sheetSha256": hashlib.sha256(SHEET.read_bytes()).hexdigest()}
    (OUT / "preflight.json").write_text(json.dumps(report, indent=2) + "\n")
    if errors:
        print("\n".join(errors)); return 1
    rows = sheet["natives"]["rows"]
    code = ["#pragma once", "#include <cstdint>", "#include <cstddef>",
            "struct NativeSpec { const char* name; uint64_t hash; uint32_t expectedRva; const char* purpose; };", "inline constexpr NativeSpec kNatives[] = {"]
    for row in rows:
        rva = "0" if row["expectedRva"] == "capture-only" else row["expectedRva"]
        code.append(f'    {{"{row["id"]}", {row["hash"]}ULL, {rva}U, "{row["purpose"]}"}},')
    code += ["};", "inline constexpr size_t kNativeCount = sizeof(kNatives) / sizeof(kNatives[0]);",
             "struct ProbeSetting { const char* id; const char* value; };", "inline constexpr ProbeSetting kSettings[] = {"]
    for row in sheet["settings"]["rows"]:
        code.append("    {" + json.dumps(row["id"]) + ", " + json.dumps(row["value"]) + "},")
    code += ["};", "inline const char* setting(const char* key) { for (const auto& s : kSettings) { const char *a=s.id, *b=key; while (*a && *a==*b) { ++a; ++b; } if (*a==*b) return s.value; } return \"\"; }", ""]
    (OUT / "probe_config.hpp").write_text("\n".join(code), encoding="utf-8")
    compiler = ROOT / ".tools/w64devkit/w64devkit/bin/g++.exe"
    env = os.environ.copy(); env["PATH"] = str(compiler.parent) + os.pathsep + env["PATH"]
    common = [str(compiler), "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror", "-static", "-static-libgcc", "-static-libstdc++", "-I", str(OUT), "-I", str(HERE)]
    subprocess.run(common + [str(HERE / "flow_test.cpp"), "-o", str(OUT / "flow_test.exe")], check=True, env=env)
    subprocess.run([str(OUT / "flow_test.exe")], check=True)
    subprocess.run(common + ["-shared", str(HERE / "NativeDump.cpp"), "-o", str(OUT / "NativeDump.asi"), "-lbcrypt", "-lversion", "-lshell32"], check=True, env=env)
    binary = OUT / "NativeDump.asi"; pe = pefile.PE(str(binary))
    exports = [e.name.decode() for e in pe.DIRECTORY_ENTRY_EXPORT.symbols if e.name]
    imports = [d.dll.decode() for d in pe.DIRECTORY_ENTRY_IMPORT]
    assert pe.FILE_HEADER.Machine == 0x8664 and "InitializeASI" in exports
    assert all("scripthook" not in s.lower() for s in imports)
    report.update({"binarySha256": hashlib.sha256(binary.read_bytes()).hexdigest(), "bytes": binary.stat().st_size,
                   "exports": exports, "imports": imports, "flowTests": "passed", "runtimeTest": "not run; launch permission required"})
    (OUT / "build-report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    return 0

if __name__ == "__main__":
    sys.exit(main())
