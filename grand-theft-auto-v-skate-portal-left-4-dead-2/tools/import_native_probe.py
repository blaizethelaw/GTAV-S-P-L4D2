"""Validate an address-only development capture. Never opens a running process.

Importing addresses does not verify the production invoker or gameplay, and must
not promote the runtime hook sheet to passed. No game code or content is copied.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
from runtime_audit import pe_sections

EXPECTED_VERSION = "1.0.3889.0"
EXPECTED_GAME_SHA256 = "677e4e355cfbdb13273b1d992407e3c261b3a108dc4dd5c8a0c4c1da651802e5"
SENTINELS = {
    "WAIT": (0x4EDE34FBADD967A6, 0x1678AC8),
    "PLAYER_PED_ID": (0xD80958FC74E988A6, 0xCE6BE8),
}


def sha256(path: Path) -> str:
    with path.open("rb") as stream:
        digest = hashlib.sha256()
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def parse_capture(text: str, executable_sections: list[dict]) -> list[dict]:
    reader = csv.DictReader(io.StringIO(text))
    if reader.fieldnames != ["name", "hash", "rva", "status"]:
        raise ValueError("Unexpected capture columns")
    rows, names, hashes = [], set(), set()
    for row in reader:
        name = row["name"]
        if not name or name in names:
            raise ValueError("Missing or repeated native name")
        if row["status"] != "ok":
            raise ValueError(f"Unresolved native: {name}")
        native_hash, rva = int(row["hash"], 16), int(row["rva"], 16)
        if not 0 < native_hash < 2**64 or native_hash in hashes:
            raise ValueError(f"Invalid or repeated hash: {name}")
        if not any(s["executable"] and s["rva"] <= rva < s["rva"] + s["virtualSize"]
                   for s in executable_sections):
            raise ValueError(f"Address outside executable game sections: {name}")
        names.add(name)
        hashes.add(native_hash)
        rows.append({"name": name, "hash": f"0x{native_hash:016X}", "rva": f"0x{rva:X}"})
    by_name = {r["name"]: r for r in rows}
    for name, (expected_hash, expected_rva) in SENTINELS.items():
        row = by_name.get(name)
        if row is None or int(row["hash"], 16) != expected_hash or int(row["rva"], 16) != expected_rva:
            raise ValueError(f"Build sentinel mismatch: {name}")
    return rows


def inspect_capture(run: Path, executable: Path, probe: Path, shv: Path) -> dict:
    manifest = json.loads((run / "manifest.json").read_text(encoding="utf-8-sig"))
    if not (run / "done.txt").is_file() or manifest.get("status") != "passed":
        raise ValueError("Capture did not finish successfully")
    if manifest.get("schemaVersion") != 1 or manifest.get("hostVersion") != EXPECTED_VERSION:
        raise ValueError("Capture uses an unsupported schema or game version")
    if sha256(executable) != EXPECTED_GAME_SHA256:
        raise ValueError("Installed host changed; addresses cannot be imported")
    for field, path in (("hostSha256", executable), ("probeSha256", probe), ("shvSha256", shv)):
        if str(manifest.get(field, "")).lower() != sha256(path):
            raise ValueError(f"Capture fingerprint mismatch: {field}")
    rows = parse_capture((run / "natives.csv").read_text(encoding="utf-8-sig"),
                         pe_sections(executable.read_bytes()))
    if manifest.get("nativeCount") != len(rows) or manifest.get("resolvedCount") != len(rows):
        raise ValueError("Native counts do not match the capture")
    if manifest.get("executableMemoryChecks") != len(rows):
        raise ValueError("Capture did not verify every runtime target's executable memory")
    if manifest.get("launchGuard") != "-nobattleye required":
        raise ValueError("Capture does not attest to the required Story Mode launch")
    return {
        "schemaVersion": 1,
        "hostVersion": EXPECTED_VERSION,
        "hostSha256": EXPECTED_GAME_SHA256,
        "mode": "validated-development-address-capture",
        "captureManifestSha256": sha256(run / "manifest.json"),
        "captureCsvSha256": sha256(run / "natives.csv"),
        "probeSha256": sha256(probe),
        "natives": rows,
        "runtimeReady": False,
        "limitation": "Addresses only. Production invoker, script thread, scalar/vector ABI, and gameplay remain untested.",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", type=Path)
    parser.add_argument("--game", required=True, type=Path)
    parser.add_argument("--probe", required=True, type=Path)
    parser.add_argument("--shv", required=True, type=Path)
    parser.add_argument("--output", type=Path, default=Path("evidence/native-addresses-3889.json"))
    args = parser.parse_args()
    try:
        report = inspect_capture(args.run, args.game, args.probe, args.shv)
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(1, f"Capture rejected: {error}\n")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Validated {len(report['natives'])} native addresses; gameplay remains unverified.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
