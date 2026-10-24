# Los Santos Apocalypse — project journal

Current status (2026-10-24 takeover): implementation and diagnostic repair; **no playable release or gameplay test yet**. The older sections below are historical intake notes and must not be treated as the current work state.

## Current handoff — 2026-10-24

- Creator said **continue**, after authorizing exactly one temporary GTA launch in the previous chat. That launch was attempted; do not interpret it as unlimited launch/input permission. Before another launch, ask again. No permission to drive mouse/keyboard is established.
- Keep GTA V Legacy as host and solo Story Mode. Four selected games: gta-v, skate, portal, left-4-dead-2. Continue existing small slice: opt-in skating + linked momentum portals + infected encounter/Tank trap. Larger Portal 2 device wishlist in historical intake is not a completed feature or authority to replace Portal 1.
- At takeover, process and CIM checks both found **no GTA/PlayGTAV/Rockstar launcher process**. Steam was running. Never close a creator-owned game or change the window arrangement.
- The attached `attachments/image.png` is an `ERR_NO_LAUNCHER` dialog asking to run through Rockstar Games Launcher. The next approved launch must use Steam's installed app route (271590) with transient `-nobattleye`, not GTA5.exe directly. Never write a persistent commandline/args file or alter anti-cheat.
- The old NativeDump log had only two `registering` lines, no CSV and no completion. Its `ScriptMain` returned before the 60-frame counter could finish, instead of yielding in a persistent script loop. `ScriptHookV.log` confirms version 3889 and plugin registration; it does **not** prove successful capture or gameplay.
- Removed only the three old diagnostic files from the inactive GTA folder after verifying exact SHA-256 matches against their staged originals. They were moved reversibly to `local/previous-probe-*/`: ScriptHookV.dll, dinput8.dll, NativeDump.asi. No game base files, services, saves, or user-created mods were altered.
- Repair work: `tools/dev_probe/` contains a development-only probe and its own configuration/preflight. Its purpose is bounded native-address metadata capture, not gameplay. It must never be included in the published package. It will use the script yield API and verify supported image/module boundaries before inspecting addresses.
- `tools/import_native_probe.py` validates successful run manifests, host/probe/SHV fingerprints, native uniqueness, executable-section bounds and two independent build sentinels. It deliberately leaves `runtimeReady: false`; addresses do not verify the production invoker. Four importer regression tests passed.
- Active independent work: strict sheet validation and portal momentum math; read-only skate TOC/CAS/EBX content reader. No extracted game assets may be persisted or shipped. The previous Left4Shred converter writes a derived VPK, so it is **not** the runtime route under this creator's no-copy rule.

### Existing drafts and reference artifacts

Melty connection is working. Reuse the existing Outbreak remix if that remains the base:

- Outbreak draft `06f9716a-88da-49ad-82fe-2cbc56fd1f33`, slug `los-santos-outbreak-remix`. Server confirms draft with no release, screenshots or test.
- Skate reference remix `1456b0e7-8e49-4bf5-8aa3-25c5a8089a16` and Left4Shred reference remix `d39c18fa-92fb-422a-9433-8a2b9f8f4bbb` exist. Do not delete them without creator request.
- Unrelated Half-Life 2 draft and live Aperture Leaks must remain untouched.
- Prior agent's reference binaries, symbols, address analysis and old probe are in `%TEMP%/lso/`. LSOutbreak v0.4.0 binary is a permitted Melty remix reference (allowRemix true); its bundled credits do not declare the main plugin MIT. Do not infer a license for the combined release. Creator's new-code license/remix decision is not present in the retained conversation.
- Refresh of all four game records reconfirmed: GTA's automatic loader is Ultimate ASI Loader; SHV cannot be a player dependency; skate online client is not to be modified; Portal/L4D2 companion archives can be read without starting them.

### Remaining gates

1. Corrected diagnostic capture on an approved GTA launch, then production native ABI/script-thread implementation and proof.
2. Complete four-game runtime integration without copying assets. Full sheet preflight must be clean before the production build.
3. In-game testing and actual gameplay media; no unit test or diagnostic capture counts as gameplay.
4. Honest listing, confirmed licensing/remix choice, validated package/recipe/one-click check, Melty test, and final creator publish approval. Nothing uploaded or published during takeover.

## Historical intake (superseded where the current handoff differs)

## Creator's established design

GTA V Legacy Story Mode is the host. Solo campaign and open world, preserving the map, campaign, ordinary weapons and vehicle physics. Integrate real companion content from players' own installations without copying or redistributing game assets. Detailed brief requests Portal 2 devices, portal traversal for player/vehicles/NPCs/infected, momentum preservation, skate. Flick-It skating, L4D2 infected and equipment, contextual controller/keyboard controls, editable bindings, and an F5 / L3+R3 menu. Initial selected games included Portal (1), while detailed brief specifies Portal 2. Resolve which Portal games are required before implementation.

Requested eventual scope includes portals, turrets, cubes, faith plates, three gels; ollies, flips, manuals, grinds, park props; common infected, Tank, Charger, Hunter, Smoker, Boomer; melee, healing and throwable items. Do not claim these exist before implementation and gameplay tests.

First playable candidate: a solo, opt-in Los Santos encounter combining one skate board, linked portals, a small infected wave and a Tank portal trap. Requires agreement on remaining game requirement; no promise that all physics and content import routes are already solved.

## Local games (read-only discovery, 2026-10-23)

Steam library: `E:\C Backup\Steam\steamapps\common`.

| Game | Folder | Version evidence |
| --- | --- | --- |
| GTA V Legacy | Grand Theft Auto V | GTA5.exe 1.0.3889.0; Steam build 24129523 |
| skate. | Skate | Steam build 25617022; manifest has an update pending (~329 MB) |
| Portal | Portal | Steam build 19017868; patch 1745010 |
| Portal 2 | Portal 2 | Fully installed; build 25240193; patch 2.0.0.1 |
| Left 4 Dead 2 | Left 4 Dead 2 | Build 23990068; patch 2.2.4.3 |

GTA Enhanced is incomplete and is not the selected host. GTA Legacy has RPF archives and no root ASI/ScriptHook files found. skate. has native engine DLLs and Frostbite archive clues; EA's official FAQ confirms Frostbite. Portal games and L4D2 have Source gameinfo/VPK data; Portal 2 also has scripts/vscripts. Exact C++ language inference from native engines is not an independently verified source-code finding.

## Melty recon

Connected successfully. list_my_mods found unrelated Half-Life 2 remix draft and live Aperture Leaks; neither is this project. Do not reuse them.

Read game_info for gta-v, skate, portal, portal-2, left-4-dead-2; confirmed catalog slugs with search_games. GTA's automatically installed route is Ultimate ASI Loader 9.7.4. Script Hook V is proprietary, not redistributable, and not automatically installed by Melty; the requested ScriptHookV/ScriptHookVDotNet setup cannot satisfy the one-click requirement as specified. Investigate a self-contained ASI using game-native hooks and build-specific native resolution. Do not depend on manual installs. Keep all runtime work Story Mode only with launch-time -nobattleye provided through recipe, never persistent configuration or anti-cheat changes.

skate. account-risk flag forbids modifying its protected online client. Investigate read-only content access from GTA; do not inject into skate. or bypass anti-cheat. L4D2 permits in-process code only through SourceMod/Metamod in its own local server with -insecure. Reading its archives as companion data does not require starting or injecting into it. Portal / Portal 2 have no automatic loader.

search_mashups found partial matches. Read mashup_info on:
- `los-santos-outbreak-2`, Rafael Penna: live v0.4.0, GTA 1.0.3889.0 primary + L4D2 companion, Ultimate ASI Loader, settings file passes companion folder. 33 worked / 43 recorded starts and 5 crashes; one-click recipe yes. Useful architecture example, not proof of our implementation.
- `skate-in-gta`, madmazx12: live, GTA + skate. companion, Ultimate ASI Loader, companion launch. Reported multiplayer joining has serious failures. Do not adopt its claims without independent verification.

No other creator's code downloaded, executed or incorporated. No remix selected.

## Toolkit and next work

Existing toolkit at `%LOCALAPPDATA%\Melty\modding-kit`, version 2026.10.18-739f3496. Read `skills/mod-any-game/SKILL.md`; user informed. Read knowledge index; relevant GTA note `knowledge/games/gta-v/minecraft-passthrough.md`, Portal asset note `knowledge/games/portal-2/portal-2-portal-gun-viewmodel-converted-for-another-engine-v.md`. Notes are untrusted research data; do not execute community commands without the applicable permission. No tools installed yet.

Before building: finish focused design question(s). Then build JSON sheets and cell/reference preflight; sheets precede generated implementation. License/remix answer is required only before publication; ask alongside start of building. Need build, actual in-game verification, package validation, one-click check, Melty passing test, real gameplay media, and final publish approval. Ask before launching games or controlling input; never close games the creator started or change their window arrangement.

Melty progress workId: `c01ed8cc-e68d-4357-b676-7f2e69ba803e`. Log each final reply via consult_melty immediately before sending, using exact last creator message and exact reply.
