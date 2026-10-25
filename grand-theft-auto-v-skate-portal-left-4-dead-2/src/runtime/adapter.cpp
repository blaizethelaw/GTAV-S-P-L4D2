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

// The pattern points to a function that initializes the native table or similar,
// but we need to resolve the RIP-relative jump or extract the actual function pointer properly.
// For safety, we will leave the Invoke stubbed for now until the exact hooking mechanism is robust.

static WaitFn g_originalWait = nullptr;
static std::vector<std::function<void()>> g_tickCallbacks;
static std::mutex g_tickMutex;
static bool g_isReady = false;
static uintptr_t g_gameBase = 0;
static uintptr_t g_waitTarget = 0;
static NativeRegistrationHandler g_nativeRegistration = nullptr;

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

    // We cannot blindly cast the pattern address to a function pointer as it might be a LEA instruction.
    // We must parse the RIP-relative offset. For now, to prevent a crash, we only use the hardcoded RVA.
    g_waitTarget = g_gameBase + 0x1678AC8;

    if (g_waitTarget && MH_CreateHook(reinterpret_cast<void*>(g_waitTarget), reinterpret_cast<void*>(&HookedWait), reinterpret_cast<void**>(&g_originalWait)) == MH_OK) {
        MH_EnableHook(reinterpret_cast<void*>(g_waitTarget));
        g_isReady = true;
    }
}

bool IsReady() {
    return g_isReady;
}

void Invoke(NativeHash hash, NativeContext* ctx) {
    // A proper native invoker requires extracting the registration table safely.
    // For now, this is a no-op to prevent executing arbitrary memory and crashing.
}

void SetTick(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(g_tickMutex);
    g_tickCallbacks.push_back(callback);
}

void Shutdown() {
    g_isReady = false;
    if (g_waitTarget) {
        MH_DisableHook(reinterpret_cast<void*>(g_waitTarget));
    }
    MH_Uninitialize();
}

} // namespace lsa::runtime
