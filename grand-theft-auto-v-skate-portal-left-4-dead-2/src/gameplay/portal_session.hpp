#pragma once
#include "portal_math.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace lsa::portal {
// The adapter supplies an identity that changes whenever a host handle is reused.
// One occupied vehicle is one actor; its riders must not also be listed as peds.
struct ActorSample {
    std::uint64_t identity{};
    Motion motion{};
    double radius{}; // Conservative sphere about motion.position.
};
struct Move {
    ActorSample actor;
    Motion destination;
    unsigned entryPortal{};
};
class Host {
public:
    virtual ~Host() = default;
    // Must validate the same live identity, destination sphere and travel sweep.
    virtual bool exitClear(const Move&) = 0;
    // Must apply position, velocity and orientation atomically on GTA's script
    // thread; report false if identity changed or movement could not complete.
    virtual bool move(const Move&) = 0;
};
struct TickResult {
    unsigned moved{}, obstructed{}, failed{}, rejected{};
};
class Session {
public:
    bool place(unsigned slot, const Frame&);
    void clear() noexcept;
    const std::optional<Frame>& portal(unsigned slot) const;
    TickResult tick(double timeSeconds, bool activeStorySession,
                    const std::vector<ActorSample>&, Host&);
private:
    struct Track { Vec3 previous; double sampledAt{}, cooldownUntil{}; };
    std::array<std::optional<Frame>, 2> portals_;
    std::unordered_map<std::uint64_t, Track> tracked_;
    std::optional<double> lastTick_;
};
} // namespace lsa::portal
