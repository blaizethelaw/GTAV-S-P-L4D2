#include "adapter.hpp"
#include <windows.h>
#include <vector>
#include <mutex>
#include "minhook.h"

namespace lsa::runtime {

using WaitFn = void(__cdecl*)(uint32_t);
static WaitFn g_originalWait = nullptr;
static std::vector<std::function<void()>> g_tickCallbacks;
static std::mutex g_tickMutex;
static bool g_isReady = false;
static uintptr_t g_gameBase = 0;

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

    // GTA V 1.0.3889 WAIT RVA
    void* waitTarget = reinterpret_cast<void*>(g_gameBase + 0x1678AC8);

    if (MH_CreateHook(waitTarget, reinterpret_cast<void*>(&HookedWait), reinterpret_cast<void**>(&g_originalWait)) == MH_OK) {
        MH_EnableHook(waitTarget);
        g_isReady = true;
    }
}

bool IsReady() {
    return g_isReady;
}

void Invoke(NativeHash hash, NativeContext* ctx) {
    // We cannot call natives without the NativeDump.asi output.
    // For now we do not invoke natives, but this function satisfies the adapter.hpp signature.
}

void SetTick(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(g_tickMutex);
    g_tickCallbacks.push_back(callback);
}

void Shutdown() {
    g_isReady = false;
    MH_DisableHook(reinterpret_cast<void*>(g_gameBase + 0x1678AC8));
    MH_Uninitialize();
}

} // namespace lsa::runtime
