#pragma once
#include "probe_config.hpp"

// Runs on the registered script fiber. Each warmup frame explicitly yields;
// returning from ScriptMain is not used as a substitute for yielding.
template<class Api> bool captureInventory(Api& api, unsigned warmupFrames) {
    for (unsigned i = 0; i < warmupFrames; ++i) {
        api.wait(0);
        if ((i + 1) % 15 == 0) api.progress("warmup", i + 1);
    }
    // Fail before collecting any inventory if either independent sentinel fails.
    for (const auto& spec : kNatives) {
        if (spec.expectedRva) {
            uint32_t rva = 0;
            if (!api.resolve(spec, rva) || rva != spec.expectedRva) {
                api.fail("sentinel validation failed", spec.name);
                return false;
            }
        }
    }
    for (size_t i = 0; i < kNativeCount; ++i) {
        uint32_t rva = 0;
        if (!api.resolve(kNatives[i], rva) || !api.write(kNatives[i], rva)) {
            api.fail("inventory capture failed", kNatives[i].name);
            return false;
        }
        api.progress("captured", i + 1);
        api.wait(0);
    }
    return api.finish();
}
