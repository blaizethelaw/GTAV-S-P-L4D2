#pragma once
#include <string>
#include <vector>
#include "../content/skate_content.hpp"

namespace lsa::skate_logic {

class SkateManager {
public:
    void Initialize(const std::string& gameRoot);
    void ToggleSkateboard();
    void Update();

private:
    bool m_isSkating = false;
    std::vector<lsc::content::SkateTrick> m_tricks;

    void ApplyPhysics();
    void HandleInput();
};

} // namespace lsa::skate_logic
