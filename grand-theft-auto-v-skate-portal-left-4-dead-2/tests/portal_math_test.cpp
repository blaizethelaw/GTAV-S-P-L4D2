#include "../src/gameplay/portal_math.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>

using namespace lsa::portal;
namespace {
int checks = 0;
void require(bool condition, const char* what) {
    ++checks;
    if (!condition) { std::cerr << "FAILED: " << what << '\n'; std::exit(1); }
}
bool near(double a, double b) { return std::abs(a-b) <= 1e-8; }
bool near(Vec3 a, Vec3 b) { return length(a-b) <= 1e-8; }
}
int main() {
    const Frame wall = *makeFrame({0,0,0},{0,1,0},{0,0,1});
    const Frame floor = *makeFrame({30,40,5},{0,0,1},{0,1,0});
    const Frame tilted = *makeFrame({-2,5,12},{1,2,3},{0,0,1});
    require(validFrame(wall) && validFrame(floor) && validFrame(tilted), "construct orthonormal frames");
    require(!makeFrame({0,0,0},{0,0,0},{0,0,1}), "reject zero normal");
    require(!makeFrame({0,0,0},{0,0,1},{0,0,4}), "reject parallel up hint");
    Frame reflected = wall; reflected.right = reflected.right * -1;
    require(!validFrame(reflected), "reject left-handed frame");
    std::mt19937 random(3889);
    std::uniform_real_distribution<double> d(-100,100);
    for (int i=0; i<1000; ++i) {
        Motion m{{d(random),d(random),d(random)}, {d(random),d(random),d(random)}, {1,0,0}, {0,0,1}};
        auto outward = transformMotion(wall,tilted,m,0);
        require(outward.has_value(), "valid motion transforms");
        auto home = transformMotion(tilted,wall,*outward,0);
        require(home.has_value() && near(home->position,m.position), "roundtrip position");
        require(near(home->velocity,m.velocity), "roundtrip velocity");
        require(near(length(outward->velocity),length(m.velocity)), "preserve speed");
        require(near(dot(outward->forward,outward->up),dot(m.forward,m.up)), "preserve orientation angle");
    }
    Motion falling{{0,-0.1,0},{0,-12,0},{0,-1,0},{0,0,1}};
    auto launch = transformMotion(wall,floor,falling,0.05);
    require(launch && near(launch->velocity,Vec3{0,0,12}), "wall entry launches out of floor");
    require(near(launch->position.z,5.15), "exit clearance points out of destination");
    auto noClearance = transformMotion(wall,floor,falling,0);
    require(near(launch->velocity,noClearance->velocity), "clearance never changes speed");
    require(!transformMotion(wall,floor,falling,-1), "negative exit clearance rejected");
    falling.velocity.x = std::numeric_limits<double>::infinity();
    require(!transformMotion(wall,floor,falling,0), "infinite velocity rejected");
    auto hit = crossing(wall,{1.6,2.8},{0,5,0},{0,-5,0},0.4);
    require(hit && near(hit->segmentFraction,0.5) && near(hit->worldHit,Vec3{}), "swept high-speed center crossing");
    require(!crossing(wall,{1.6,2.8},{0,-5,0},{0,5,0},0.4), "backward crossing rejected");
    require(!crossing(wall,{1.6,2.8},{0,0,0},{0,-5,0},0.4), "starting on plane does not retrigger");
    require(!crossing(wall,{1.6,2.8},{2,5,0},{2,-5,0},0.4), "outside aperture rejected");
    require(!crossing(wall,{1.6,2.8},{0,5,0},{0,-5,0},1.6), "oversized actor rejected");
    require(!crossing(wall,{1.6,2.8},{0,5,0},{0,4,0},0.4), "no plane crossing rejected");
    require(!crossing(wall,{1.6,2.8},{1.15,5,2.3},{1.15,-5,2.3},0.4), "ellipse corner clipping rejected");
    require(!crossing(wall,{1.6,2.8},{0,5,0},{0,-5,0},-0.1), "negative radius rejected");
    std::cout << "Portal geometry: " << checks << " checks passed; no host game started.\n";
}
