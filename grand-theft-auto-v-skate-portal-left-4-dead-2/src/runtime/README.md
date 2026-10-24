# GTA V Legacy runtime adapter research

Status: **not implemented and not playable**. No ASI has been built, installed, or executed. The executable and companion game files remain untouched.

The intended route is an original Story Mode ASI loaded by Melty's Ultimate ASI Loader. It must resolve game native functions, run callbacks on the game script thread, preserve the active script context, and refuse unsupported hosts. This avoids a dependency on proprietary Script Hook V. It is a route to investigate, not an established working replacement.

Planned interface, not yet an implemented API:

- `Initialize(log)` performs exact host/version checks and installs only verified hooks.
- `IsReady()` is true only after all host acceptance checks pass.
- `Invoke(hash, args)` is allowed only within the verified game-thread callback.
- `SetTick(callback)` registers one non-blocking callback per game frame.
- `Shutdown()` removes only this adapter's changes and restores original functions.

Implementation gates are the rows in `sheets/hooks.json`. The pending cells intentionally prevent an ASI build. `tools/runtime_audit.py` reads the installed executable without opening a game process and records metadata plus candidate-pattern matches. It neither modifies the executable nor copies game bytes into the project. Candidate signatures are observations from public source, not proof that the installed build uses those addresses.

## Findings on this installation

The installed GTA5.exe is reported by Windows as 1.0.3889.0, length 47,467,128 bytes, SHA-256 `677e4e355cfbdb13273b1d992407e3c261b3a108dc4dd5c8a0c4c1da651802e5`.

All four required current public Cfx scripting patterns tested against executable sections have zero matches in the static file: native registry, script tick, active script TLS, and script collection. An additional older registry candidate also had zero matches in the initial investigation. This does not prove the route impossible; runtime code may differ from the on-disk image. It does mean no known target address or calling convention has been verified here. No offsets are guessed.

Still required before host implementation:

1. A supported, verified game-thread hook for this exact build.
2. Current-build native hash mapping and bounded registry traversal.
3. Verified script context and scalar/vector native-call behavior.
4. A Story Mode guard, including the transient `-nobattleye` launch option, that never edits anti-cheat services/files or persistent launch configuration.
5. In-game checks that the unmodified campaign continues and callback removal restores the game normally.

## Primary sources and reuse decisions

- [ScriptHookVDotNet README](https://github.com/scripthookvdotnet/scripthookvdotnet): requires Alexander Blade's C++ Script Hook V. It does not solve the one-click dependency problem by itself.
- [Cfx script engine source](https://github.com/citizenfx/fivem/blob/master/code/components/rage-scripting-five/src/scrEngine.cpp): evidence for candidate pattern names, native registration, game-thread context, and version-dependent locations. No code was copied. The [current root license](https://github.com/citizenfx/fivem/blob/master/LICENSE) does not include `rage-scripting-five` in its listed LGPL directories; do not silently vendor it.
- [OpenVHook](https://github.com/ivanmeler/OpenVHook): GPL-2.0 source alternative, with a version list ending at 2245 in the inspected ScriptEngine.cpp. Initialization also contains launcher-check patches. Do not install or vendor it unchanged; those changes are outside this task and must not be used.
- [Give-Two/ScriptHookV](https://github.com/Give-Two/ScriptHookV): another GPL-2.0 open implementation, with no verified 3889 compatibility found in this investigation. A public repository alone does not prove this build works.

No third-party runtime code or proprietary content was incorporated. The audit utility is original Python using the standard library. Three unit tests passed for binary wildcard matching, overlapping matches, malformed signatures, PE bounds, and the distinction between file offsets and RVAs. Those tests establish the audit's basic behavior; they are not gameplay tests.
