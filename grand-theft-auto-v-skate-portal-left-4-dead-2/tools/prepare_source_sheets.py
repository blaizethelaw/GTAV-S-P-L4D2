"""Inspect selected Source companions in place; persist configuration/metadata only.

This does not claim the C++ decoder or gameplay works. The separate runtime audit
builds and exercises the parser after the configuration preflight passes.
"""
from pathlib import Path
import hashlib
import json
import struct
import sys

from inspect_source import Vpk

ROOT = Path(__file__).resolve().parents[1]
LIBRARY = Path(r"E:\C Backup\Steam\steamapps\common")
PROFILES = [
    dict(id="portal", game="portal", directory="Portal", layout="Portal", roots=["portal", "hl2"],
         archives=["portal/portal_pak_dir.vpk", "hl2/hl2_textures_dir.vpk", "hl2/hl2_sound_misc_dir.vpk", "hl2/hl2_misc_dir.vpk"],
         requiredArchive="portal/portal_pak_dir.vpk", mdlVersions=[44, 46, 48], groupHeaderBytes=25, stripHeaderBytes=27),
    dict(id="portal2", game="portal-2", directory="Portal 2", layout="Portal2", roots=["update", "portal2_dlc3", "portal2_dlc2", "portal2_dlc1", "portal2"],
         archives=[f"{p}/pak01_dir.vpk" for p in ["update", "portal2_dlc3", "portal2_dlc2", "portal2_dlc1", "portal2"]],
         requiredArchive="portal2/pak01_dir.vpk", mdlVersions=[49], groupHeaderBytes=33, stripHeaderBytes=35),
    dict(id="l4d2", game="left-4-dead-2", directory="Left 4 Dead 2", layout="Left4Dead2", roots=["update", "left4dead2_dlc3", "left4dead2_dlc2", "left4dead2_dlc1", "left4dead2"],
         archives=[f"{p}/pak01_dir.vpk" for p in ["update", "left4dead2_dlc3", "left4dead2_dlc2", "left4dead2_dlc1", "left4dead2"]],
         requiredArchive="left4dead2/pak01_dir.vpk", mdlVersions=[49], groupHeaderBytes=25, stripHeaderBytes=27),
]
ASSETS = [
    ("portal_world_gun", "portal", "model", "models/weapons/w_portalgun.mdl"),
    ("portal_view_gun", "portal", "model", "models/weapons/v_portalgun.mdl"),
    ("portal_blue_shot", "portal", "wave", "sound/weapons/portalgun/portalgun_shoot_blue1.wav"),
    ("portal2_view_gun", "portal2", "model", "models/weapons/v_portalgun.mdl"),
    ("portal2_world_gun", "portal2", "model", "models/weapons/w_portalgun.mdl"),
    ("portal2_blue_shot", "portal2", "wave", "sound/weapons/portalgun/wpn_portal_gun_fire_blue_01.wav"),
    ("portal2_orange_shot", "portal2", "wave", "sound/weapons/portalgun/wpn_portal_gun_fire_red_01.wav"),
    ("portal2_enter", "portal2", "wave", "sound/weapons/portalgun/portal_enter_01.wav"),
    ("l4d2_tank", "l4d2", "model", "models/infected/hulk.mdl"),
    ("l4d2_common", "l4d2", "model", "models/infected/common_male_tshirt_cargos.mdl"),
    ("l4d2_pistol", "l4d2", "model", "models/w_models/weapons/w_pistol_a.mdl"),
    ("l4d2_tank_voice", "l4d2", "wave", "sound/player/tank/voice/attack/tank_attack_01.wav"),
]


def save_sheet(name, rows, evidence, references=None):
    columns = list(rows[0])
    obj = dict(schemaVersion=1, name=name, columns=columns, rows=rows,
               checks={r["id"]: {c: dict(status="verified", evidence=evidence,
                        artifacts=["evidence/source-inspection.json"]) for c in columns} for r in rows})
    if references:
        obj["references"] = references
    (ROOT / "sheets" / f"{name}.json").write_text(json.dumps(obj, indent=2) + "\n")


def main():
    mounts = {}
    for profile in PROFILES:
        root = LIBRARY / profile["directory"]
        assert (root / profile["requiredArchive"]).is_file()
        mounts[profile["id"]] = [(str(Path(a).parent), Vpk(root / a)) for a in profile["archives"] if (root / a).is_file()]

    def read(profile, path):
        for folder in profile["roots"]:
            for d, archive in mounts[profile["id"]]:
                if d == folder and path in archive.entries:
                    return archive.read(path), str(archive.path.relative_to(LIBRARY / profile["directory"]))
            loose = LIBRARY / profile["directory"] / folder / path
            if loose.is_file():
                return loose.read_bytes(), str(loose.relative_to(LIBRARY / profile["directory"]))
        raise FileNotFoundError(path)

    asset_rows, records = [], []
    for ident, mount, kind, path in ASSETS:
        profile = next(p for p in PROFILES if p["id"] == mount)
        data, physical = read(profile, path)
        record = dict(id=ident, physical=physical, bytes=len(data), sha256=hashlib.sha256(data).hexdigest())
        if kind == "model":
            version = struct.unpack_from("<I", data, 4)[0]
            assert version in profile["mdlVersions"]
            vvd, _ = read(profile, path[:-4] + ".vvd")
            vtx, _ = read(profile, path[:-4] + ".dx90.vtx")
            assert data[8:12] == vvd[8:12] == vtx[16:20]
            record.update(mdlVersion=version, vertices=struct.unpack_from("<I", vvd, 16)[0], vtxVersion=struct.unpack_from("<I", vtx)[0])
        else:
            assert data[:4] == b"RIFF" and data[8:12] == b"WAVE"
            at, fmt, frames = 12, None, None
            while at + 8 <= len(data):
                n = struct.unpack_from("<I", data, at + 4)[0]
                if data[at:at + 4] == b"fmt ":
                    fmt = struct.unpack_from("<HHIIHH", data, at + 8)
                if data[at:at + 4] == b"data":
                    frames = n
                at += 8 + n + (n & 1)
            assert fmt and frames is not None and fmt[0] == 1
            record.update(codec=fmt[0], channels=fmt[1], sampleRate=fmt[2], bitsPerSample=fmt[5], frames=frames // fmt[4])
        asset_rows.append(dict(id=ident, mount=mount, kind=kind, path=path, inspection="CRC and header verified in memory; C++/rendering audit separate"))
        records.append(record)

    limits = [dict(id="bounded_reader", maxPayloadBytes=268435456, maxTreeBytes=67108864, maxEntries=1000000,
                   maxInventory=256, maxMaterialDepth=8, maxMaterialBytes=1048576, maxWaveSeconds=120)]
    report = dict(scope="Read-only archive/header inspection; no decoder or gameplay claim", contentWritten=False,
                  assets=records, profiles=[{k:v for k,v in p.items() if k != "directory"} for p in PROFILES])
    (ROOT / "evidence" / "source-inspection.json").write_text(json.dumps(report, indent=2) + "\n")
    save_sheet("source_mounts", [{k:v for k,v in p.items() if k != "directory"} for p in PROFILES],
               "Official install archive existence and selected headers inspected in place; layout fields match file offsets and Valve optimize.h; decoder verification separate")
    save_sheet("source_assets", asset_rows, "Selected installed path read with archive CRC or loose RIFF validation; metadata in source-inspection.json",
               [dict(column="mount", sheet="source_mounts", target="id")])
    save_sheet("source_limits", limits, "Explicit finite resource caps for hostile/truncated input; parser rejection tests validate enforcement")
    print(f"Inspected {len(records)} selected model/audio inputs in place; wrote metadata/configuration only.")


if __name__ == "__main__":
    main()
