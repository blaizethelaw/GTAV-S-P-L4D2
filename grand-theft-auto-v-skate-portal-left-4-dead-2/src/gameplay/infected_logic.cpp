#include "infected_logic.hpp"
#include "../runtime/adapter.hpp"

namespace lsa::infected_logic {

void Director::Update() {
    // Stub: Calculate intensity based on player action

    if (m_intensity > 100.0f) {
        SpawnWave();
        m_intensity = 0.0f;
    }
}

void Director::SpawnWave() {
    lsa::runtime::NativeContext ctx{};
    // Stub: Use GET_PLAYER_PED (0x43A66C31C68491C0) to find player
    // Use SET_ENTITY_COORDS to place infected near player
}

void Director::SpawnSpecial(const std::string& type) {
    // Stub: Spawn specific special infected model (e.g. Tank)
}

} // namespace lsa::infected_logic
