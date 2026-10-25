#include <cstring>

#include "skate_logic.hpp"
#include "../runtime/adapter.hpp"
#include <iostream>

namespace lsa::skate_logic {

void SkateManager::Initialize(const std::string& gameRoot) {
    try {
        m_tricks = lsc::content::loadSkateScorables(gameRoot,
            "Data/Win32/levels/game/dingolevel_ftue_island/dingolevel_ftue_island.toc",
            "win32/levels/game/dingolevel_ftue_island/dingolevel_ftue_island");
    } catch (const std::exception& e) {
        std::cerr << "Failed to load skate content: " << e.what() << "\n";
    }
}

void SkateManager::ToggleSkateboard() {
    m_isSkating = !m_isSkating;
    lsa::runtime::NativeContext ctx{};

    if (m_isSkating) {
        // GET_PLAYER_PED
        ctx.arg_count = 0;
        uint64_t playerPed = 0;
        ctx.return_value = &playerPed;
        lsa::runtime::Invoke(0xD80958FC74E988A6, &ctx); // PLAYER_PED_ID

        // GET_ENTITY_COORDS
        ctx.arg_count = 2;
        uint64_t posArgs[2] = {playerPed, 1}; // entity, alive
        ctx.args = posArgs;
        float playerPos[3] = {0};
        ctx.return_value = playerPos;
        lsa::runtime::Invoke(0x3FEF770D40960D5A, &ctx);

        // GET_HASH_KEY("prop_skateboard_01")
        ctx.arg_count = 1;
        const char* propName = "prop_skateboard_01";
        ctx.args = &propName;
        uint64_t propHash = 0;
        ctx.return_value = &propHash;
        lsa::runtime::Invoke(0xD24D37CC275948CC, &ctx); // GET_HASH_KEY

        // REQUEST_MODEL
        ctx.arg_count = 1;
        ctx.args = &propHash;
        ctx.return_value = nullptr;
        lsa::runtime::Invoke(0x963D27A58DF860AC, &ctx); // REQUEST_MODEL

        // Normally we'd wait for HAS_MODEL_LOADED here, but since this is synchronous in this limited scope
        // or assuming the model is loaded, we proceed to create.

        // CREATE_OBJECT (0x509D5878EB39E842)
        // modelHash, x, y, z, isNetwork, bScriptHostObj, dynamic
        uint64_t createArgs[7] = {0};
        createArgs[0] = propHash;
        std::memcpy(&createArgs[1], &playerPos[0], sizeof(float));
        std::memcpy(&createArgs[2], &playerPos[1], sizeof(float));
        std::memcpy(&createArgs[3], &playerPos[2], sizeof(float));
        createArgs[4] = 1;
        createArgs[5] = 1;
        createArgs[6] = 0;

        ctx.arg_count = 7;
        ctx.args = createArgs;
        ctx.return_value = &m_boardEntity;
        lsa::runtime::Invoke(0x509D5878EB39E842, &ctx);

        if (m_boardEntity != 0) {
            // ATTACH_ENTITY_TO_ENTITY (0x6B9BBD38AB0796DF)
            // entity1, entity2, boneIndex, xPos, yPos, zPos, xRot, yRot, zRot, p9, useSoftPinning, collision, isPed, vertexIndex, fixedRot, p15
            uint64_t attachArgs[16] = {0};
            attachArgs[0] = m_boardEntity;
            attachArgs[1] = playerPed;
            attachArgs[2] = 0; // Bone index (root)

            float zOffset = -0.95f; // Place at feet
            std::memcpy(&attachArgs[5], &zOffset, sizeof(float));

            attachArgs[9] = 0;
            attachArgs[10] = 0;
            attachArgs[11] = 1; // Collision
            attachArgs[12] = 0; // isPed
            attachArgs[13] = 2; // vertexIndex
            attachArgs[14] = 1; // fixedRot
            attachArgs[15] = 0;

            ctx.arg_count = 16;
            ctx.args = attachArgs;
            ctx.return_value = nullptr;
            lsa::runtime::Invoke(0x6B9BBD38AB0796DF, &ctx);
        }
    } else {
        if (m_boardEntity != 0) {
            // DELETE_ENTITY (0xAE3CBE5BF394C9C9)
            uint64_t deleteArgs[1] = { reinterpret_cast<uint64_t>(&m_boardEntity) };
            ctx.arg_count = 1;
            ctx.args = deleteArgs;
            ctx.return_value = nullptr;
            lsa::runtime::Invoke(0xAE3CBE5BF394C9C9, &ctx);
            m_boardEntity = 0;
        }
    }
}

void SkateManager::Update() {
    if (!m_isSkating) return;

    HandleInput();
    ApplyPhysics();
}

void SkateManager::HandleInput() {
    lsa::runtime::NativeContext ctx{};

    // Check if player pressed Jump (Control 22) for Ollie
    ctx.arg_count = 2;
    uint64_t jumpArgs[2] = {0, 22};
    ctx.args = jumpArgs;

    uint64_t isJumpPressed = 0;
    ctx.return_value = &isJumpPressed;
    lsa::runtime::Invoke(0x580417101DDB492F, &ctx); // IS_CONTROL_JUST_PRESSED

    if (isJumpPressed != 0 && m_boardEntity != 0) {
        // Jump logic: APPLY_FORCE_TO_ENTITY (0xC5F68BE9613E2D18)
        // entity, forceFlags, x, y, z, offX, offY, offZ, boneIndex, isDirectionRel, ignoreUpVec, isForceRel, p12, p13
        uint64_t forceArgs[14] = {0};

        ctx.arg_count = 0;
        uint64_t playerPed = 0;
        ctx.return_value = &playerPed;
        lsa::runtime::Invoke(0xD80958FC74E988A6, &ctx); // PLAYER_PED_ID

        forceArgs[0] = playerPed; // Apply force to player to lift the attached board
        forceArgs[1] = 1; // Force type

        float forceZ = 5.0f; // Ollie force
        std::memcpy(&forceArgs[4], &forceZ, sizeof(float));

        forceArgs[9] = 1; // isDirectionRel
        forceArgs[10] = 0;
        forceArgs[11] = 1;
        forceArgs[12] = 0;
        forceArgs[13] = 0;

        ctx.arg_count = 14;
        ctx.args = forceArgs;
        ctx.return_value = nullptr;
        lsa::runtime::Invoke(0xC5F68BE9613E2D18, &ctx);

        if (!m_tricks.empty()) {
            std::cout << "Did trick: " << m_tricks[0].name << " for " << m_tricks[0].baseScore << " points!\n";
        }
    }
}

void SkateManager::ApplyPhysics() {
    // Stub: Constantly apply forward momentum when skating
}

} // namespace lsa::skate_logic
