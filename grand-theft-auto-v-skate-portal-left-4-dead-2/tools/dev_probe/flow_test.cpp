#include "flow.hpp"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

struct FakeApi {
    unsigned waits=0, resolves=0, writes=0, failures=0, finishes=0;
    unsigned failAt=0; bool wrongSentinel=false, writeFails=false;
    std::vector<std::string> resolved;
    void wait(unsigned) { ++waits; }
    void progress(const char*, size_t) {}
    void fail(const char*, const char*) { ++failures; }
    bool resolve(const NativeSpec& spec, uint32_t& rva) {
        assert(waits >= 60); ++resolves; resolved.emplace_back(spec.name);
        rva = spec.expectedRva ? spec.expectedRva : 0x1000U + resolves;
        if (wrongSentinel) ++rva;
        return !failAt || resolves != failAt;
    }
    bool write(const NativeSpec&, uint32_t) { ++writes; return !writeFails; }
    bool finish() { ++finishes; return true; }
};
int main() {
    FakeApi ok; assert(captureInventory(ok, 60));
    assert(ok.waits == 60 + kNativeCount && ok.writes == kNativeCount);
    assert(ok.resolves == kNativeCount + 2 && ok.finishes == 1 && ok.failures == 0);
    assert(ok.resolved[0] == "WAIT" && ok.resolved[1] == "PLAYER_PED_ID");
    FakeApi badSentinel; badSentinel.wrongSentinel = true;
    assert(!captureInventory(badSentinel, 60)); assert(badSentinel.writes == 0 && badSentinel.finishes == 0);
    FakeApi noAddress; noAddress.failAt = 4;
    assert(!captureInventory(noAddress, 60)); assert(noAddress.writes == 1 && noAddress.finishes == 0);
    FakeApi io; io.writeFails = true;
    assert(!captureInventory(io, 60)); assert(io.finishes == 0 && io.failures == 1);
    std::puts("PASS: real wait loop, independent sentinels first, bounded inventory, address failure, write failure.");
}
