#pragma once

namespace lsa::ui_logic {

class UIManager {
public:
    void Update();
private:
    bool m_menuOpen = false;
    void DrawMenu();
};

} // namespace lsa::ui_logic
