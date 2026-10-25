#include "../src/gameplay/portal_session.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace lsa::portal;
void require(bool ok, const char* why) { if (!ok) { std::cerr << why << '\n'; std::exit(1); } }
struct TestHost final : Host {
    bool clear=true, success=true; unsigned attempted=0; std::optional<Move> last;
    bool exitClear(const Move&) override { return clear; }
    bool move(const Move& m) override { ++attempted; last=m; return success; }
};
void pair(Session& s) {
    require(s.place(0,*makeFrame({0,0,0},{0,1,0},{0,0,1})),"place wall");
    require(s.place(1,*makeFrame({30,0,10},{0,0,1},{0,1,0})),"place floor");
}
ActorSample actor(double y, std::uint64_t id=1) {
    return {id,{{0,y,0},{0,-20,0},{0,-1,0},{0,0,1}},0.4};
}
int main() {
    Session s; TestHost h; pair(s);
    require(s.tick(1,true,{actor(1)},h).moved==0,"first sample must not teleport");
    require(s.tick(1.1,true,{actor(-1)},h).moved==1,"crossing moves once");
    require(h.attempted==1 && std::abs(h.last->destination.velocity.z-20)<1e-9,"preserve rotated speed");
    require(h.last->destination.position.z>11.4,"exit includes body radius");
    require(s.tick(1.15,true,{actor(1)},h).moved==0,"cooldown sampling");
    require(s.tick(1.2,true,{actor(-1)},h).moved==0,"cooldown rejects repeated crossing");
    s.clear(); pair(s); h.clear=false;
    s.tick(2,true,{actor(1)},h);
    require(s.tick(2.1,true,{actor(-1)},h).obstructed==1 && h.attempted==1,"blocked exit never calls move");
    h.clear=true; h.success=false; s.tick(2.2,true,{actor(1)},h);
    require(s.tick(2.3,true,{actor(-1)},h).failed==1,"host failure is not counted as success");
    h.success=true; s.tick(2.4,true,{actor(1)},h);
    require(s.tick(2.5,true,{actor(-1)},h).moved==1,"failed move does not start cooldown");
    s.clear(); pair(s); s.tick(3,true,{actor(1)},h);
    require(s.tick(3.1,true,{actor(-1,2)},h).moved==0,"reused host handle needs a new identity");
    s.clear(); pair(s); s.tick(4,true,{actor(1)},h);
    require(s.tick(5,true,{actor(-1)},h).moved==0,"stale samples do not teleport");
    s.tick(5.1,true,{actor(1)},h); s.place(0,*makeFrame({0,0,0},{0,1,0},{0,0,1}));
    require(s.tick(5.2,true,{actor(-1)},h).moved==0,"replacement clears old samples");
    s.tick(5.3,false,{},h); require(!s.portal(0) && !s.portal(1),"inactive session resets portals");
    pair(s); s.tick(6,true,{actor(1)},h);
    auto duplicate=s.tick(6.1,true,{actor(-1),actor(-1)},h);
    require(duplicate.moved==0 && duplicate.rejected==2,"duplicate actor identity rejected");
    s.clear(); pair(s); s.tick(7,true,{actor(1)},h); s.tick(7.1,true,{},h);
    require(s.tick(7.2,true,{actor(-1)},h).moved==0,"disappearing actor removed from history");
    s.clear(); pair(s); s.tick(8,true,{actor(1)},h);
    require(s.tick(7,true,{actor(-1)},h).moved==0,"clock reset clears samples");
    std::cout << "Portal coordinator: crossing, clearance, momentum, cooldown, identity and session reset passed; GTA adapter not tested.\n";
}
