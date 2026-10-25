#include "infected_logic.hpp"
#include "../runtime/adapter.hpp"
#include <cstring>

namespace lsa::infected_logic {

void Director::Update() {
    // Basic spawning loop based on a simple timer/intensity logic
    m_intensity += 1.0f;

    if (m_intensity > 300.0f) {
        SpawnWave();
        m_intensity = 0.0f;
    }
}

void Director::SpawnWave() {
    // We will spawn a basic "infected" using a random ped model and force it to attack the player
    SpawnSpecial("a_m_m_skater_01"); // Placeholder for common infected
}

void Director::SpawnSpecial(const std::string& type) {
    lsa::runtime::NativeContext ctx{};

    // GET_PLAYER_PED (0xD80958FC74E988A6)
    ctx.arg_count = 0;
    uint64_t playerPed = 0;
    ctx.return_value = &playerPed;
    lsa::runtime::Invoke(0xD80958FC74E988A6, &ctx);
    if (!playerPed) return;

    // GET_ENTITY_COORDS (0x3FEF770D40960D5A)
    ctx.arg_count = 2;
    uint64_t posArgs[2] = {playerPed, 1};
    ctx.args = posArgs;
    float playerPos[3] = {0};
    ctx.return_value = playerPos;
    lsa::runtime::Invoke(0x3FEF770D40960D5A, &ctx);

    // GET_HASH_KEY
    ctx.arg_count = 1;
    const char* modelType = type.c_str();
    ctx.args = &modelType;
    uint64_t modelHash = 0;
    ctx.return_value = &modelHash;
    lsa::runtime::Invoke(0xD24D37CC275948CC, &ctx);

    // REQUEST_MODEL
    ctx.arg_count = 1;
    ctx.args = &modelHash;
    ctx.return_value = nullptr;
    lsa::runtime::Invoke(0x963D27A58DF860AC, &ctx);

    // CREATE_PED (0xD49F9B0955C367DE)
    // pedType, modelHash, x, y, z, heading, isNetwork, bScriptHostPed
    uint64_t createArgs[8] = {0};
    createArgs[0] = 26; // PED_TYPE_CRIMINAL
    createArgs[1] = modelHash;

    // Offset spawn position slightly away from player
    float spawnX = playerPos[0] + 5.0f;
    float spawnY = playerPos[1] + 5.0f;
    float spawnZ = playerPos[2];

    std::memcpy(&createArgs[2], &spawnX, sizeof(float));
    std::memcpy(&createArgs[3], &spawnY, sizeof(float));
    std::memcpy(&createArgs[4], &spawnZ, sizeof(float));

    float heading = 0.0f;
    std::memcpy(&createArgs[5], &heading, sizeof(float));

    createArgs[6] = 1; // isNetwork
    createArgs[7] = 0; // bScriptHostPed

    ctx.arg_count = 8;
    ctx.args = createArgs;
    uint64_t newPed = 0;
    ctx.return_value = &newPed;
    lsa::runtime::Invoke(0xD49F9B0955C367DE, &ctx);

    if (newPed != 0) {
        // TASK_COMBAT_PED (0xF166E48407BAC484)
        // ped, targetPed, combatFlags, threatResponseFlags
        uint64_t combatArgs[4] = {0};
        combatArgs[0] = newPed;
        combatArgs[1] = playerPed;
        combatArgs[2] = 0;
        combatArgs[3] = 16;

        ctx.arg_count = 4;
        ctx.args = combatArgs;
        ctx.return_value = nullptr;
        lsa::runtime::Invoke(0xF166E48407BAC484, &ctx);
    }
}

} // namespace lsa::infected_logic
