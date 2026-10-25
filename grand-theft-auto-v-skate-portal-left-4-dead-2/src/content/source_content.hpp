#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// Original, read-only parser. Game bytes are read from the player's installation
// into process memory. There is deliberately no extraction or cache-writing API.
namespace lsc::content {
using Bytes = std::vector<std::uint8_t>;
struct ContentError : std::runtime_error { using std::runtime_error::runtime_error; };

class SourceContent {
public:
    virtual ~SourceContent() = default;
    virtual bool contains(const std::string& virtualPath) const = 0;
    virtual Bytes read(const std::string& virtualPath) const = 0;
};

class VpkArchive final : public SourceContent {
public:
    explicit VpkArchive(const std::filesystem::path& directoryFile);
    bool contains(const std::string& virtualPath) const override;
    Bytes read(const std::string& virtualPath) const override; // validates complete-file CRC32
    std::vector<std::string> inventory(const std::string& prefix, std::size_t limit = 64) const;
    std::size_t size() const noexcept { return entries_.size(); }
private:
    struct Entry {
        std::uint32_t crc{}, offset{}, length{};
        std::uint16_t archive{};
        Bytes preload;
    };
    std::filesystem::path directoryFile_;
    std::uint64_t embeddedStart_{}, embeddedLength_{};
    std::unordered_map<std::string, Entry> entries_;
};

enum class SourceGame { Portal, Portal2, Left4Dead2 };
// Official archive mounts only, in the sheet's patch/DLC/base precedence order.
// Loose game sounds are read in place; custom/addons are never scanned. No game
// DLL is loaded and no files are cached or written into either installation.
class SourceInstallation final : public SourceContent {
public:
    SourceInstallation(const std::filesystem::path& gameRoot, SourceGame game);
    bool contains(const std::string& virtualPath) const override;
    Bytes read(const std::string& virtualPath) const override;
    std::size_t archiveCount() const noexcept;
    SourceGame game() const noexcept { return game_; }
private:
    struct Mount { std::unique_ptr<VpkArchive> archive; std::filesystem::path looseRoot; };
    std::filesystem::path gameRoot_;
    SourceGame game_;
    std::vector<Mount> mounts_;
};

// Only a relative game-content path is accepted; canonicalization also prevents
// symlink/reparse-point escape. Nothing is written to the companion folder.
Bytes readLoose(const std::filesystem::path& contentRoot, const std::string& virtualPath);

enum class VtxLayout { Portal, Left4Dead2, Portal2 };
struct Vertex {
    std::array<float, 3> position{}, normal{}, weights{};
    std::array<float, 2> uv{};
    std::array<std::uint8_t, 3> bones{};
    std::uint8_t boneCount{};
};
struct DrawRange {
    std::uint32_t firstIndex{}, indexCount{}, material{};
};
struct Bone {
    std::string name;
    std::int32_t parent{};
    std::array<float, 3> localPosition{};
    std::array<float, 4> localQuaternion{};
    std::array<float, 12> poseToBone{};
};
struct Model {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<DrawRange> draws;
    std::vector<std::string> materials; // virtual VMT paths, resolved when loaded
    std::vector<Bone> bones;
    std::uint32_t checksum{};
};
// LOD0; one model per bodygroup (selection defaults to model 0). Vertices remain
// in Source units/bind pose. Renderer owns transforms, animation and game physics.
Model decodeModel(const Bytes& mdl, const Bytes& vvd, const Bytes& vtx,
                  VtxLayout layout, unsigned skin = 0,
                  const std::vector<unsigned>& bodySelections = {});
Model loadModel(const SourceContent&, const std::string& mdlPath,
                VtxLayout layout, unsigned skin = 0,
                const std::vector<unsigned>& bodySelections = {});

enum class TextureFormat { Rgba8, Bgra8, Bc1, Bc2, Bc3 };
struct Texture {
    unsigned width{}, height{};
    TextureFormat format{};
    Bytes pixels; // top mip, first frame, compressed for BC1/2/3; GPU-ready
};
struct Material {
    std::string shader, baseTexturePath;
    bool translucent{}, alphaTest{};
    Texture baseTexture;
};
Texture decodeVtf(const Bytes&);
Material loadMaterial(const SourceContent&, const std::string& vmtPath);

// PCM audio ready for an in-memory sound backend. Unsupported codecs fail
// explicitly; no executable or external conversion process is started.
struct WaveAudio {
    unsigned channels{}, sampleRate{}, bitsPerSample{}, frames{};
    Bytes pcm;
};
WaveAudio decodeWave(const Bytes&);
WaveAudio loadWave(const SourceContent&, const std::string& wavPath);
} // namespace lsc::content
