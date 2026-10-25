#include "portal_session.hpp"
#include "../generated/portal_runtime.hpp"
#include <cmath>
#include <stdexcept>
#include <unordered_set>

namespace lsa::portal {
namespace {
using Config = sheets::portal_runtime::traversal;
bool finite(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
bool valid(const ActorSample& a) {
    return a.identity != 0 && std::isfinite(a.radius) && a.radius > 0
        && a.radius < Config::halfWidthMeters && finite(a.motion.position)
        && finite(a.motion.velocity) && finite(a.motion.forward) && finite(a.motion.up)
        && length(a.motion.forward) > 0 && length(a.motion.up) > 0;
}
}
bool Session::place(unsigned slot, const Frame& frame) {
    if (slot >= portals_.size() || !validFrame(frame)) return false;
    portals_[slot] = frame;
    tracked_.clear(); // Replacing a portal never sweeps actors across its old plane.
    return true;
}
void Session::clear() noexcept {
    portals_ = {}; tracked_.clear(); lastTick_.reset();
}
const std::optional<Frame>& Session::portal(unsigned slot) const {
    if (slot >= portals_.size()) throw std::out_of_range("portal slot");
    return portals_[slot];
}
TickResult Session::tick(double now, bool active,
                         const std::vector<ActorSample>& actors, Host& host) {
    TickResult result;
    if (!active || !std::isfinite(now) || now < 0) { clear(); return result; }
    if (lastTick_ && now <= *lastTick_) { tracked_.clear(); lastTick_=now; return result; }
    lastTick_=now;
    if (!portals_[0] || !portals_[1]) { tracked_.clear(); return result; }
    std::unordered_set<std::uint64_t> seen, duplicates;
    for (const auto& a : actors) if (!seen.insert(a.identity).second) duplicates.insert(a.identity);
    seen.clear();
    std::size_t accepted=0;
    for (const auto& actor : actors) {
        if (!valid(actor) || duplicates.count(actor.identity)
            || accepted >= static_cast<std::size_t>(Config::maxActors)) {
            ++result.rejected; continue;
        }
        ++accepted; seen.insert(actor.identity);
        auto found=tracked_.find(actor.identity);
        if (found==tracked_.end()) {
            tracked_.emplace(actor.identity, Track{actor.motion.position, now, 0}); continue;
        }
        auto& track=found->second;
        const Vec3 previous=track.previous;
        const double elapsed=now-track.sampledAt;
        track.previous=actor.motion.position; track.sampledAt=now;
        if (elapsed > Config::maxSampleGapSeconds || now < track.cooldownUntil) continue;
        for (unsigned entry=0;entry<2;++entry) {
            const auto& from=*portals_[entry]; const auto& to=*portals_[1-entry];
            const Aperture aperture{Config::halfWidthMeters,Config::halfHeightMeters};
            if (!crossing(from,aperture,previous,actor.motion.position,actor.radius)) continue;
            // Move the actor's entire bounding sphere beyond the exit plane.
            auto destination=transformMotion(from,to,actor.motion,
                                              actor.radius+Config::exitPaddingMeters);
            if (!destination) { ++result.rejected; break; }
            Move move{actor,*destination,entry};
            if (!host.exitClear(move)) { ++result.obstructed; break; }
            if (!host.move(move)) { ++result.failed; break; }
            ++result.moved; track.previous=destination->position;
            track.cooldownUntil=now+Config::cooldownSeconds;
            break;
        }
    }
    for (auto it=tracked_.begin();it!=tracked_.end();) {
        if (!seen.count(it->first)) it=tracked_.erase(it); else ++it;
    }
    return result;
}
} // namespace lsa::portal
