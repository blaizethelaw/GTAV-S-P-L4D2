#include <cstring>

#include "portal_logic.hpp"
#include "../runtime/adapter.hpp"

namespace lsa::portal_logic {

void PortalManager::ShootPortal(bool isBlue) {
    lsa::runtime::NativeContext ctx{};

    // Get gameplay cam coords
    ctx.arg_count = 0;
    lsa::runtime::Invoke(0x14D6F5678D8F1B37, &ctx); // GET_GAMEPLAY_CAM_COORD

    float camCoord[3];
    if (ctx.return_value) {
        std::memcpy(camCoord, ctx.return_value, sizeof(float)*3);
    } else {
        camCoord[0] = 0; camCoord[1] = 0; camCoord[2] = 0;
    }

    // Get gameplay cam rot
    ctx.arg_count = 1;
    uint64_t rotOrder = 2;
    ctx.args = &rotOrder;
    lsa::runtime::Invoke(0x837765A25378F0BB, &ctx); // GET_GAMEPLAY_CAM_ROT

    float camRot[3];
    if (ctx.return_value) {
        std::memcpy(camRot, ctx.return_value, sizeof(float)*3);
    } else {
        camRot[0] = 0; camRot[1] = 0; camRot[2] = 0;
    }

    // START_SHAPE_TEST_LOS_PROBE
    uint64_t probeArgs[7] = {0};
    // (Stub: In full implementation, do proper vector math to calculate end coords based on rot, then shape test)

    // Dummy values representing raycast hit for now since we don't have math utils yet
    lsa::portal::Vec3 hitPos = {camCoord[0] + 5, camCoord[1], camCoord[2]};
    lsa::portal::Vec3 normal = {0, -1, 0};
    ShootPortal(isBlue, hitPos, normal);
}

void PortalManager::ShootPortal(bool isBlue, const lsa::portal::Vec3& hitPos, const lsa::portal::Vec3& normal) {
    Portal& p = isBlue ? bluePortal : orangePortal;

    // Default up vector for testing, would normally cross with player heading
    lsa::portal::Vec3 up = {0, 0, 1};
    if (std::abs(normal.z) > 0.9) {
        up = {1, 0, 0};
    }

    auto frame = lsa::portal::makeFrame(hitPos, normal, up);
    if (frame) {
        p.frame = *frame;
        p.active = true;
    }
}

void PortalManager::DrawPortal(const Portal& portal, int r, int g, int b) {
    if (!portal.active) return;

    // Use dynamically resolved DRAW_MARKER via the adapter
    lsa::runtime::NativeContext ctx{};

    // Create an array to hold all 24 64-bit arguments for DRAW_MARKER (hash 0x28477EC23D892089)
    uint64_t args[24] = {0};

    // type(28 for cylinder)
    args[0] = 28;

    // Position (x, y, z)
    float px = portal.frame.center.x;
    float py = portal.frame.center.y;
    float pz = portal.frame.center.z;
    std::memcpy(&args[1], &px, sizeof(float));
    std::memcpy(&args[2], &py, sizeof(float));
    std::memcpy(&args[3], &pz, sizeof(float));

    // Direction (x, y, z) - not used for type 28

    // Rotation (x, y, z) - facing the portal normal
    float rx = portal.frame.normal.x * 90.0f;
    float ry = portal.frame.normal.y * 90.0f;
    float rz = portal.frame.normal.z * 90.0f;
    std::memcpy(&args[7], &rx, sizeof(float));
    std::memcpy(&args[8], &ry, sizeof(float));
    std::memcpy(&args[9], &rz, sizeof(float));

    // Scale (x, y, z)
    float scale = 1.0f;
    std::memcpy(&args[10], &scale, sizeof(float));
    std::memcpy(&args[11], &scale, sizeof(float));
    std::memcpy(&args[12], &scale, sizeof(float));

    // Color (r, g, b, a)
    args[13] = r;
    args[14] = g;
    args[15] = b;
    args[16] = 255;

    // Flags (bobUpAndDown, faceCamera, p19, rotate, textureDict, textureName, drawOnEnts)
    args[17] = 0; // no bob
    args[18] = 0; // no face camera
    args[19] = 2; // default
    args[20] = 0; // no rotate
    args[21] = 0; // null string
    args[22] = 0; // null string
    args[23] = 0; // false

    ctx.arg_count = 24;
    ctx.args = args;

    lsa::runtime::Invoke(0x28477EC23D892089, &ctx);
}

void PortalManager::Update() {
    if (bluePortal.active) {
        DrawPortal(bluePortal, 0, 0, 255); // Blue
    }
    if (orangePortal.active) {
        DrawPortal(orangePortal, 255, 165, 0); // Orange
    }

    if (bluePortal.active && orangePortal.active) {
        lsa::runtime::NativeContext ctx{};
        ctx.arg_count = 0;
        lsa::runtime::Invoke(0xD80958FC74E988A6, &ctx); // PLAYER_PED_ID
        uint64_t playerPed = reinterpret_cast<uint64_t>(ctx.return_value);

        ctx.arg_count = 2;
        uint64_t posArgs[2] = {playerPed, 1}; // entity, alive
        ctx.args = posArgs;
        lsa::runtime::Invoke(0x3FEF770D40960D5A, &ctx); // GET_ENTITY_COORDS

        // (Stub: Read ctx.return_value as Vector3, compare against portal.frame.center)
        // (Stub: Apply transformMotion if crossing occurs and SET_ENTITY_COORDS)
    }
}

} // namespace lsa::portal_logic
