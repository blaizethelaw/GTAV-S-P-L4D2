#include "minhook.h"

// Note: To successfully hook functions, a real MinHook implementation must be linked.
// This stub prevents compilation failures in CI/CD environments without MinHook binaries,
// but it will NOT intercept engine events if compiled directly.

MH_STATUS WINAPI MH_Initialize(void) { return MH_OK; }
MH_STATUS WINAPI MH_Uninitialize(void) { return MH_OK; }
MH_STATUS WINAPI MH_CreateHook(void* pTarget, void* pDetour, void** ppOriginal) { return MH_OK; }
MH_STATUS WINAPI MH_EnableHook(void* pTarget) { return MH_OK; }
MH_STATUS WINAPI MH_DisableHook(void* pTarget) { return MH_OK; }
