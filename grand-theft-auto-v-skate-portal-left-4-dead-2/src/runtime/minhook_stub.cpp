#include "minhook.h"

MH_STATUS WINAPI MH_Initialize(void) { return MH_OK; }
MH_STATUS WINAPI MH_Uninitialize(void) { return MH_OK; }
MH_STATUS WINAPI MH_CreateHook(void* pTarget, void* pDetour, void** ppOriginal) { return MH_OK; }
MH_STATUS WINAPI MH_EnableHook(void* pTarget) { return MH_OK; }
MH_STATUS WINAPI MH_DisableHook(void* pTarget) { return MH_OK; }
