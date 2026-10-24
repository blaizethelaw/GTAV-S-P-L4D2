#pragma once

#include "source_content.hpp"
#include <optional>

namespace lsc::content {
// Values come from the player's EBX data at run time. No score table or catalog
// is compiled into the mod, and no game data is written to a cache.
struct SkateTrick {
    std::string sourceAsset, name, category, difficulty;
    std::int32_t baseScore{}, continuousScore{};
    bool continuous{}, hidden{};
};

// Narrow, self-describing RIFF/EBX reader: rejects unfamiliar schemas rather
// than treating guessed offsets as game rules. Returns null for group assets.
std::optional<SkateTrick> decodeSkateScorable(const Bytes& ebx,
                                           const std::string& sourceAsset);

// sourceToc and bundleName come from the design sheet. Current known source is
// Data/Win32/levels/game/dingolevel_ftue_island/dingolevel_ftue_island.toc;
// bundle win32/levels/game/dingolevel_ftue_island/dingolevel_ftue_island.
// Only selected EBX scorable payloads are decoded; all reads stay in the install.
std::vector<SkateTrick> loadSkateScorables(const std::filesystem::path& gameRoot,
                                        const std::string& sourceToc,
                                        const std::string& bundleName);
} // namespace lsc::content
