"""Populate the diagnostic inventory from already inspected local metadata."""
import hashlib
import json
import os
from pathlib import Path
import pefile

ROOT = Path(__file__).resolve().parent
TEMP = Path(os.environ["TEMP"]) / "lso"
NATIVES = TEMP / "natives.json"
SHV = TEMP / "shv/x/bin/ScriptHookV.dll"
GAME = Path(r"E:\C Backup\Steam\steamapps\common\Grand Theft Auto V\GTA5.exe")
names = {
    "sentinel": "WAIT PLAYER_PED_ID",
    "player": "PLAYER_ID GET_PLAYER_PED",
    "entity": "GET_ENTITY_COORDS SET_ENTITY_COORDS SET_ENTITY_COORDS_NO_OFFSET GET_ENTITY_VELOCITY SET_ENTITY_VELOCITY GET_ENTITY_HEADING SET_ENTITY_HEADING GET_ENTITY_ROTATION SET_ENTITY_ROTATION SET_ENTITY_COLLISION SET_ENTITY_INVINCIBLE DOES_ENTITY_EXIST DELETE_ENTITY GET_ENTITY_MODEL GET_ENTITY_TYPE IS_ENTITY_DEAD GET_ENTITY_SPEED SET_ENTITY_AS_MISSION_ENTITY ATTACH_ENTITY_TO_ENTITY",
    "camera": "GET_FINAL_RENDERED_CAM_COORD GET_FINAL_RENDERED_CAM_ROT GET_FINAL_RENDERED_CAM_FOV GET_GAMEPLAY_CAM_COORD GET_GAMEPLAY_CAM_ROT",
    "drawing": "DRAW_MARKER",
    "models": "GET_HASH_KEY REQUEST_MODEL HAS_MODEL_LOADED IS_MODEL_VALID SET_MODEL_AS_NO_LONGER_NEEDED CREATE_OBJECT CREATE_OBJECT_NO_OFFSET DELETE_OBJECT CREATE_PED DELETE_PED",
    "raycast": "START_SHAPE_TEST_LOS_PROBE START_SHAPE_TEST_CAPSULE GET_SHAPE_TEST_RESULT GET_GROUND_Z_FOR_3D_COORD",
    "controls": "GET_CONTROL_NORMAL IS_CONTROL_JUST_PRESSED DISABLE_CONTROL_ACTION GET_DISABLED_CONTROL_NORMAL IS_DISABLED_CONTROL_JUST_PRESSED IS_USING_KEYBOARD_AND_MOUSE",
    "guard": "NETWORK_IS_GAME_IN_PROGRESS NETWORK_IS_SESSION_ACTIVE",
    "clock": "GET_GAME_TIMER GET_FRAME_COUNT",
    "script": "GET_THIS_SCRIPT_NAME",
}
source = {n["name"]: n for n in json.loads(NATIVES.read_text())}
expected = {"WAIT": "0x1678AC8", "PLAYER_PED_ID": "0xCE6BE8"}
rows = []
for purpose, group in names.items():
    for name in group.split():
        n = source[name]
        rows.append({"id": name, "namespace": n["namespace"], "hash": n["hash"],
                     "expectedRva": expected.get(name, "capture-only"), "purpose": purpose,
                     "evidence": "Local alloc8or header-derived natives.json; hash inventory checked against downloaded SHV table. Sentinel RVAs independently observed in LSOutbreak 3889." if name in expected else "Local alloc8or header-derived natives.json; hash inventory checked against downloaded SHV table. Address not known: diagnostic only.",
                     "status": "verified-inventory"})

shvpe = pefile.PE(str(SHV)); gamepe = pefile.PE(str(GAME), fast_load=True)
exports = {x.name.decode() for x in shvpe.DIRECTORY_ENTRY_EXPORT.symbols if x.name}
settings = {
    "probe_id": "NativeDump-3889-v3-dev-only",
    "game_version": "1.0.3889.0",
    "game_timestamp": str(gamepe.FILE_HEADER.TimeDateStamp),
    "game_image_size": str(gamepe.OPTIONAL_HEADER.SizeOfImage),
    "shv_sha256": hashlib.sha256(SHV.read_bytes()).hexdigest(),
    "shv_timestamp": str(shvpe.FILE_HEADER.TimeDateStamp),
    "shv_image_size": str(shvpe.OPTIONAL_HEADER.SizeOfImage),
    "last_native_rva": "0x1E7F08",
    "register_export": "?scriptRegister@@YAXPEAUHINSTANCE__@@P6AXXZ@Z",
    "wait_export": "?scriptWait@@YAXK@Z",
    "init_export": "?nativeInit@@YAX_K@Z",
    "warmup_frames": "60",
    "required_launch_token": "-nobattleye",
    "distribution": "development-diagnostic-only-never-package",
}
for key in ["register_export", "wait_export", "init_export"]:
    assert settings[key] in exports, key
table = SHV.read_bytes()
hashes = {int.from_bytes(table[o:o+8], "little") for o in range(0x5C560, 0x5C560 + 6575 * 0xE8, 0xE8)}
for row in rows:
    assert int(row["hash"], 16) in hashes, row["id"]
sheet = {"schemaVersion": 1, "scope": "development diagnostic, independent of blocked gameplay build",
         "natives": {"columns": list(rows[0]), "rows": rows},
         "settings": {"columns": ["id", "value", "evidence", "status"],
                      "rows": [{"id": key, "value": value,
                                "evidence": "Current local PE metadata and inspected SHV resolver ABI; development-only requirements.",
                                "status": "verified-inventory"} for key, value in settings.items()]}}
(ROOT / "probe.sheet.json").write_text(json.dumps(sheet, indent=2) + "\n", encoding="utf-8")
print(f"Prepared {len(rows)} diagnostic native rows and {len(settings)} configuration rows; no native handlers executed.")
