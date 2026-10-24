#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// Original, read-only parser. Game bytes are read from the player's installation
// into process memory. There is deliberately no extraction or cache-writing API.
namespace lsc::content {
using Bytes = std::vector<std::uint8_t>;
struct ContentError : std::runtime_error { using std::runtime_error::runtime_error; };

class VpkArchive {
public:
    explicit VpkArchive(const std::filesystem::path& directoryFile);
    bool contains(const std::string& virtualPath) const;
    Bytes read(const std::string& virtualPath) const; // validates complete-file CRC32
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

// Only a relative game-content path is accepted; canonicalization also prevents
// symlink/reparse-point escape. Nothing is written to the companion folder.
Bytes readLoose(const std::filesystem::path& contentRoot, const std::string& virtualPath);

enum class VtxLayout { Left4Dead2, Portal2 };
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
Model loadModel(const VpkArchive&, const std::string& mdlPath,
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
Material loadMaterial(const VpkArchive&, const std::string& vmtPath);
} // namespace lsc::content
