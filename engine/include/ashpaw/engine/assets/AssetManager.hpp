#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "ashpaw/engine/math/Vector2.hpp"

namespace ashpaw::engine::assets {

struct VisualRect {
    math::Rect bounds {};
    math::Color color {};
    std::int32_t z {0};
};

enum class LayerDrawOrder {
    Background = 0,
    Midground = 1,
    Foreground = 2
};

struct MapLayer {
    std::string name;
    LayerDrawOrder drawOrder {LayerDrawOrder::Midground};
    std::int32_t z {0};
    std::vector<VisualRect> visuals;
};

struct MapMarker {
    std::string id;
    std::string type;
    std::string label;
    math::Vector2 position {};
    math::Color color {};
    std::int32_t z {0};
};

struct WorldPackageManifest {
    std::string mapId;
    std::string packageVersion;
    std::string contentHash;
};

struct MapData {
    std::string name;
    math::Vector2 worldSize {};
    math::Vector2 spawnPoint {};
    std::int32_t spawnZ {0};
    std::vector<MapLayer> layers;
    std::vector<math::Rect> blockers;
    std::vector<MapMarker> markers;
};

class AssetManager {
public:
    void SetAssetRoot(std::filesystem::path root);
    [[nodiscard]] std::filesystem::path Resolve(const std::filesystem::path& path) const;
    [[nodiscard]] MapData LoadMap(const std::filesystem::path& path) const;
    [[nodiscard]] WorldPackageManifest LoadPackageManifestForMap(const std::filesystem::path& path) const;

private:
    std::filesystem::path assetRoot_ {"assets"};
};

}  // namespace ashpaw::engine::assets
