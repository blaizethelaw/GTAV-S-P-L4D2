"""Read-only evidence for the GTA adapter. Never opens a process or loads a DLL.

The PE and pattern observations here establish only static facts. A match does
not prove a call signature, thread context, or runtime behavior is correct.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
from typing import Any


def pe_sections(data: bytes) -> list[dict[str, Any]]:
    if len(data) < 64 or data[:2] != b"MZ":
        raise ValueError("Not a Windows PE image")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if pe + 24 > len(data) or data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("Invalid PE header")
    machine, count = struct.unpack_from("<HH", data, pe + 4)
    if machine != 0x8664:
        raise ValueError("Expected an x64 image")
    optional_size = struct.unpack_from("<H", data, pe + 20)[0]
    table = pe + 24 + optional_size
    if table + count * 40 > len(data):
        raise ValueError("Truncated section table")
    sections = []
    for index in range(count):
        start = table + 40 * index
        name = data[start:start + 8].rstrip(b"\0").decode("ascii", errors="replace")
        virtual_size, rva, raw_size, raw_offset = struct.unpack_from("<IIII", data, start + 8)
        characteristics = struct.unpack_from("<I", data, start + 36)[0]
        if raw_size and raw_offset + raw_size > len(data):
            raise ValueError("Section extends past the file")
        sections.append({"name": name, "rva": rva, "virtualSize": virtual_size,
                         "rawOffset": raw_offset, "rawSize": raw_size,
                         "executable": bool(characteristics & 0x20000000)})
    return sections


def find_pattern(data: bytes, signature: str) -> list[int]:
    tokens = signature.split()
    if not tokens or all(token in ("?", "??") for token in tokens):
        raise ValueError("Pattern must contain at least one literal byte")
    pieces = []
    for token in tokens:
        if token in ("?", "??"):
            pieces.append(b".")
        elif len(token) == 2 and re.fullmatch(r"[0-9a-fA-F]{2}", token):
            pieces.append(re.escape(bytes.fromhex(token)))
        else:
            raise ValueError("Pattern token must be a byte or wildcard")
    # Lookahead retains overlapping matches, which matter when uniqueness gates a hook.
    pattern = re.compile(b"(?=" + b"".join(pieces) + b")", re.DOTALL)
    return [match.start() for match in pattern.finditer(data)]


def audit(executable: Path, sheet: Path) -> dict[str, Any]:
    data = executable.read_bytes()
    sections = pe_sections(data)
    document = json.loads(sheet.read_text(encoding="utf-8"))
    observations = []
    for row in document["rows"]:
        signature = row.get("candidateSignature")
        if not signature or signature == "not-applicable":
            continue
        hits = []
        for section in sections:
            if not section["executable"]:
                continue
            start = section["rawOffset"]
            contents = data[start:start + section["rawSize"]]
            for offset in find_pattern(contents, signature):
                hits.append({"section": section["name"],
                             "fileOffset": start + offset,
                             "rva": section["rva"] + offset})
        observations.append({"hookId": row["id"], "matchCount": len(hits), "hits": hits,
                             "runtimeVerified": False})
    return {"schemaVersion": 1, "mode": "read-only-static-file-scan",
            "executable": str(executable), "length": len(data),
            "sha256": hashlib.sha256(data).hexdigest(), "sections": sections,
            "observations": observations, "runtimeReady": False,
            "limitation": "No process was started or inspected. Static matches never authorize hooks."}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--sheet", type=Path, default=Path("sheets/hooks.json"))
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report = json.dumps(audit(args.executable, args.sheet), indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report, encoding="utf-8")
    else:
        print(report, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
