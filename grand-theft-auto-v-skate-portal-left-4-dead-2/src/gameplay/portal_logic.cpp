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

    lsa::runtime::NativeContext ctx{};
    // Setup args for DRAW_MARKER (hash 0x28477EC23D892089)
    // type(28 for cylinder), posX, posY, posZ, dirX, dirY, dirZ, rotX, rotY, rotZ, scaleX, scaleY, scaleZ, r, g, b, a, bobUpAndDown, faceCamera, p19, rotate, textureDict, textureName, drawOnEnts
    ctx.arg_count = 24;
    // (Stub: In full implementation, populate args array and call Invoke)
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
