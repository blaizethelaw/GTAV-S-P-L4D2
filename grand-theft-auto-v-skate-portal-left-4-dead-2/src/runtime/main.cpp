#include "adapter.hpp"
#include "../gameplay/portal_logic.hpp"
#include "../gameplay/skate_logic.hpp"
#include "../gameplay/infected_logic.hpp"
#include "../gameplay/ui_logic.hpp"
#include <windows.h>
#include <memory>

std::unique_ptr<lsa::portal_logic::PortalManager> g_portalManager;
std::unique_ptr<lsa::skate_logic::SkateManager> g_skateManager;
std::unique_ptr<lsa::infected_logic::Director> g_director;
std::unique_ptr<lsa::ui_logic::UIManager> g_uiManager;

void MainTick() {
    if (g_portalManager) g_portalManager->Update();
    if (g_skateManager) g_skateManager->Update();
    if (g_director) g_director->Update();
    if (g_uiManager) g_uiManager->Update();
}

void MainInit() {
    g_portalManager = std::make_unique<lsa::portal_logic::PortalManager>();
    g_skateManager = std::make_unique<lsa::skate_logic::SkateManager>();
    g_director = std::make_unique<lsa::infected_logic::Director>();
    g_uiManager = std::make_unique<lsa::ui_logic::UIManager>();

    // In a full implementation, we'd retrieve the game path dynamically
    // g_skateManager->Initialize("E:\\C Backup\\Steam\\steamapps\\common\\Skate");

    lsa::runtime::SetTick(MainTick);
}

extern "C" __declspec(dllexport) void InitializeASI() {
    lsa::runtime::Initialize(GetModuleHandle(nullptr));
    if (lsa::runtime::IsReady()) {
        MainInit();
    }
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        InitializeASI();
    } else if (dwReason == DLL_PROCESS_DETACH) {
        lsa::runtime::Shutdown();
    }
    return TRUE;
}
