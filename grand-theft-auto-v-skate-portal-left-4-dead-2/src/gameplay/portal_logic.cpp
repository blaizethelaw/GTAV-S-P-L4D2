#include <cstring>
#include <cmath>

#include "portal_logic.hpp"
#include "../runtime/adapter.hpp"

// Convert degrees to radians
constexpr float PI = 3.14159265359f;
inline float DegToRad(float deg) { return deg * (PI / 180.0f); }

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

    // Calculate forward vector from rotation
    float rotX = DegToRad(camRot[0]);
    float rotZ = DegToRad(camRot[2]);
    float multXY = std::abs(std::cos(rotX));
    float forwardX = -std::sin(rotZ) * multXY;
    float forwardY = std::cos(rotZ) * multXY;
    float forwardZ = std::sin(rotX);

    float rayDistance = 1000.0f;
    float endCoord[3] = {
        camCoord[0] + forwardX * rayDistance,
        camCoord[1] + forwardY * rayDistance,
        camCoord[2] + forwardZ * rayDistance
    };

    // START_SHAPE_TEST_LOS_PROBE (0x7EE9F5D83DD4F90E)
    // x1, y1, z1, x2, y2, z2, flags, entity, p8
    uint64_t probeArgs[9] = {0};
    std::memcpy(&probeArgs[0], &camCoord[0], sizeof(float));
    std::memcpy(&probeArgs[1], &camCoord[1], sizeof(float));
    std::memcpy(&probeArgs[2], &camCoord[2], sizeof(float));
    std::memcpy(&probeArgs[3], &endCoord[0], sizeof(float));
    std::memcpy(&probeArgs[4], &endCoord[1], sizeof(float));
    std::memcpy(&probeArgs[5], &endCoord[2], sizeof(float));
    probeArgs[6] = -1; // flags (intersect everything)

    // Get player ped to ignore
    ctx.arg_count = 0;
    lsa::runtime::Invoke(0xD80958FC74E988A6, &ctx); // PLAYER_PED_ID
    probeArgs[7] = reinterpret_cast<uint64_t>(ctx.return_value);
    probeArgs[8] = 7; // p8

    ctx.arg_count = 9;
    ctx.args = probeArgs;
    lsa::runtime::Invoke(0x7EE9F5D83DD4F90E, &ctx); // START_SHAPE_TEST_LOS_PROBE
    uint64_t shapeTestHandle = reinterpret_cast<uint64_t>(ctx.return_value);

    // GET_SHAPE_TEST_RESULT (0x3D87450E15D98694)
    // shapeTestHandle, hit, endCoords, surfaceNormal, entityHit
    uint32_t hit = 0;
    float hitCoords[3] = {0};
    float surfaceNormal[3] = {0};
    uint64_t entityHit = 0;

    uint64_t resultArgs[5] = {
        shapeTestHandle,
        reinterpret_cast<uint64_t>(&hit),
        reinterpret_cast<uint64_t>(&hitCoords),
        reinterpret_cast<uint64_t>(&surfaceNormal),
        reinterpret_cast<uint64_t>(&entityHit)
    };

    ctx.arg_count = 5;
    ctx.args = resultArgs;
    lsa::runtime::Invoke(0x3D87450E15D98694, &ctx);

    if (hit) {
        lsa::portal::Vec3 hitPos = {hitCoords[0], hitCoords[1], hitCoords[2]};
        lsa::portal::Vec3 normal = {surfaceNormal[0], surfaceNormal[1], surfaceNormal[2]};
        ShootPortal(isBlue, hitPos, normal);
    }
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
    float pitch = std::atan2(-portal.frame.normal.z, std::sqrt(portal.frame.normal.x * portal.frame.normal.x + portal.frame.normal.y * portal.frame.normal.y)) * (180.0f / PI);
    float yaw = std::atan2(portal.frame.normal.x, portal.frame.normal.y) * (180.0f / PI);
    float roll = 0.0f; // Simplified for now

    std::memcpy(&args[7], &pitch, sizeof(float));
    std::memcpy(&args[8], &roll, sizeof(float));
    std::memcpy(&args[9], &yaw, sizeof(float));

    // Scale (x, y, z)
    float scale = 2.0f; // Make portals visible enough
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
    lsa::runtime::NativeContext ctx{};

    // Check if player pressed Attack (Control 24) to shoot Blue Portal
    ctx.arg_count = 2;
    uint64_t attackArgs[2] = {0, 24}; // 0 = Player control group, 24 = Attack
    ctx.args = attackArgs;
    lsa::runtime::Invoke(0x580417101DDB492F, &ctx); // IS_CONTROL_JUST_PRESSED
    if (reinterpret_cast<uint64_t>(ctx.return_value) != 0) {
        ShootPortal(true);
    }

    // Check if player pressed Aim (Control 25) to shoot Orange Portal
    ctx.arg_count = 2;
    uint64_t aimArgs[2] = {0, 25}; // 0 = Player control group, 25 = Aim
    ctx.args = aimArgs;
    lsa::runtime::Invoke(0x580417101DDB492F, &ctx); // IS_CONTROL_JUST_PRESSED
    if (reinterpret_cast<uint64_t>(ctx.return_value) != 0) {
        ShootPortal(false);
    }

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
