#pragma once
#include "portal_math.hpp"
#include <optional>

namespace lsa::portal_logic {

struct Portal {
    bool active = false;
    lsa::portal::Frame frame;
};

class PortalManager {
public:
    Portal bluePortal;
    Portal orangePortal;

    void ShootPortal(bool isBlue);
    void ShootPortal(bool isBlue, const lsa::portal::Vec3& hitPos, const lsa::portal::Vec3& normal);
    void Update();

private:
    void DrawPortal(const Portal& portal, int r, int g, int b);
};

} // namespace lsa::portal_logic