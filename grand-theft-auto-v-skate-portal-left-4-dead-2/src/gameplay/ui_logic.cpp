#include "ui_logic.hpp"
#include "../runtime/adapter.hpp"
#include <iostream>

namespace lsa::ui_logic {

void UIManager::Update() {
    lsa::runtime::NativeContext ctx{};

    // Check if player pressed F5 or L3+R3 (mapping this to a specific known control id, e.g. 166 for F5 usually in scripts, but using 166 for now as stub)
    ctx.arg_count = 2;
    uint64_t toggleArgs[2] = {0, 166};
    ctx.args = toggleArgs;

    uint64_t isTogglePressed = 0;
    ctx.return_value = &isTogglePressed;
    lsa::runtime::Invoke(0x580417101DDB492F, &ctx); // IS_CONTROL_JUST_PRESSED

    if (isTogglePressed != 0) {
        m_menuOpen = !m_menuOpen;
    }

    if (m_menuOpen) {
        DrawMenu();
    }
}

void UIManager::DrawMenu() {
    // Stub: Usually involves calling SET_TEXT_SCALE, SET_TEXT_COLOR, etc.
    // For now we will just print to console for development verification.
}

} // namespace lsa::ui_logic
