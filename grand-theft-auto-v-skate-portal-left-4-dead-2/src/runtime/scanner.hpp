#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace lsa::runtime {

class Scanner {
public:
    static uintptr_t FindPattern(const char* pattern, const char* mask);
};

} // namespace lsa::runtime
