#include "scanner.hpp"
#include <windows.h>
#include <psapi.h>

namespace lsa::runtime {

uintptr_t Scanner::FindPattern(const char* pattern, const char* mask) {
    MODULEINFO moduleInfo = {0};
    HMODULE hModule = GetModuleHandle(nullptr);
    if (!GetModuleInformation(GetCurrentProcess(), hModule, &moduleInfo, sizeof(MODULEINFO))) {
        return 0;
    }

    uintptr_t baseAddress = reinterpret_cast<uintptr_t>(moduleInfo.lpBaseOfDll);
    uint32_t sizeOfImage = moduleInfo.SizeOfImage;

    size_t patternLength = strlen(mask);

    for (uint32_t i = 0; i < sizeOfImage - patternLength; ++i) {
        bool found = true;
        for (size_t j = 0; j < patternLength; ++j) {
            if (mask[j] != '?' && pattern[j] != *(reinterpret_cast<const char*>(baseAddress + i + j))) {
                found = false;
                break;
            }
        }
        if (found) {
            return baseAddress + i;
        }
    }

    return 0;
}

} // namespace lsa::runtime
