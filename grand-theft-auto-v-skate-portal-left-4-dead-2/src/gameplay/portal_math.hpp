#pragma once

#include <optional>

// Host-independent geometry only. The game adapter must validate scene collision,
// ownership, current entity handles and cooldown before applying a traversal.
namespace lsa::portal {
struct Vec3 { double x{}, y{}, z{}; };
Vec3 operator+(Vec3 a, Vec3 b) noexcept;
Vec3 operator-(Vec3 a, Vec3 b) noexcept;
Vec3 operator*(Vec3 a, double scalar) noexcept;
double dot(Vec3 a, Vec3 b) noexcept;
double length(Vec3 v) noexcept;

// Orthonormal right/up/normal frame; normal points out of the portal surface.
struct Frame { Vec3 center, right, up, normal; };
struct Motion { Vec3 position, velocity, forward, up; };
struct Aperture { double halfWidth, halfHeight; };
struct Crossing { double segmentFraction; Vec3 worldHit; };

std::optional<Frame> makeFrame(Vec3 center, Vec3 normal, Vec3 upHint) noexcept;
bool validFrame(const Frame&) noexcept;
std::optional<Vec3> rotateVector(const Frame& from, const Frame& to, Vec3) noexcept;
std::optional<Motion> transformMotion(const Frame& from, const Frame& to,
                                      const Motion&, double exitClearance) noexcept;
std::optional<Crossing> crossing(const Frame&, Aperture, Vec3 previousPosition,
                                Vec3 currentPosition, double actorRadius) noexcept;
} // namespace lsa::portal
