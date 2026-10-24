#include <cstring>

#include "portal_logic.hpp"
#include "../runtime/adapter.hpp"

namespace lsa::portal_logic {

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
        // (Stub: Check collisions and apply teleportation using lsa::portal::transformMotion)
    }
}

} // namespace lsa::portal_logic
