#pragma once

#if defined(_WIN32)
#include <windows.h>
#endif

#define MH_OK 0
#define MH_ERROR_ALREADY_INITIALIZED 1
#define MH_ERROR_NOT_INITIALIZED 2
#define MH_ERROR_ALREADY_CREATED 3
#define MH_ERROR_NOT_CREATED 4
#define MH_ERROR_ENABLED 5
#define MH_ERROR_DISABLED 6
#define MH_ERROR_NOT_EXECUTABLE 7
#define MH_ERROR_UNSUPPORTED_FUNCTION 8
#define MH_ERROR_MEMORY_ALLOC 9
#define MH_ERROR_MEMORY_PROTECT 10
#define MH_ERROR_MODULE_NOT_FOUND 11
#define MH_ERROR_FUNCTION_NOT_FOUND 12

typedef int MH_STATUS;

#ifdef __cplusplus
extern "C" {
#endif

MH_STATUS WINAPI MH_Initialize(void);
MH_STATUS WINAPI MH_Uninitialize(void);
MH_STATUS WINAPI MH_CreateHook(void* pTarget, void* pDetour, void** ppOriginal);
MH_STATUS WINAPI MH_EnableHook(void* pTarget);
MH_STATUS WINAPI MH_DisableHook(void* pTarget);

#ifdef __cplusplus
}
#endif
