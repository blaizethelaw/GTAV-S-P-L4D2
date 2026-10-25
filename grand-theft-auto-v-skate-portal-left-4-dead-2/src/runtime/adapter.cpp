#include "adapter.hpp"
#include <windows.h>
#include <vector>
#include <mutex>
#include "minhook.h"
#include "scanner.hpp"
#include <iostream>

namespace lsa::runtime {

using WaitFn = void(__cdecl*)(uint32_t);
using NativeRegistrationHandler = void(__cdecl*)(NativeContext*);
using NativeInitFn = NativeRegistrationHandler(__cdecl*)(uint64_t);

static WaitFn g_originalWait = nullptr;
static std::vector<std::function<void()>> g_tickCallbacks;
static std::mutex g_tickMutex;
static bool g_isReady = false;
static uintptr_t g_gameBase = 0;
static uintptr_t g_waitTarget = 0;
static NativeInitFn g_nativeRegistration = nullptr;

void __cdecl HookedWait(uint32_t ms) {
    if (g_isReady) {
        std::lock_guard<std::mutex> lock(g_tickMutex);
        for (auto& cb : g_tickCallbacks) {
            cb();
        }
    }
    if (g_originalWait) {
        g_originalWait(ms);
    }
}

void Initialize(void* module_handle) {
    g_gameBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (!g_gameBase) return;

    MH_Initialize();

    g_waitTarget = Scanner::FindPattern("\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x8B\xD9\x8B\xFA", "xxxxxxxxxxxxxx");

    if (!g_waitTarget) {
        // GTA V 1.0.3889 WAIT RVA fallback
        g_waitTarget = g_gameBase + 0x1678AC8;
    }

    if (MH_CreateHook(reinterpret_cast<void*>(g_waitTarget), reinterpret_cast<void*>(&HookedWait), reinterpret_cast<void**>(&g_originalWait)) == MH_OK)