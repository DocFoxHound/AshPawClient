#include "ashpaw/engine/assets/AssetManager.hpp"

#include <fstream>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

namespace ashpaw::engine::assets {

namespace {

math::Color ParseColor(const nlohmann::json& colorJson, const math::Color& fallback = {}) {
    const auto colors = colorJson.is_array() ? colorJson.get<std::vector<float>>() : std::vector<float> {};
    if (colors.size() == 4) {
        return {colors[0], colors[1], colors[2], colors[3]};
    }
    return fallback;
}

LayerDrawOrder ParseDrawOrder(const std::string& drawOrder) {
    if (drawOrder == "background") {
        return LayerDrawOrder::Background;
    }
    if (drawOrder == "foreground") {
        return LayerDrawOrder::Foreground;
    }
    return LayerDrawOrder::Midground;
}

VisualRect ParseVisualRect(const nlohmann::json& visual) {
    VisualRect rect;
    rect.bounds.x = visual.value("x", 0.0F);
    rect.bounds.y = visual.value("y", 0.0F);
    rect.bounds.w = visual.value("w", 0.0F);
    rect.bounds.h = visual.value("h", 0.0F);
    rect.color = ParseColor(visual.value("color", nlohmann::json::array()), {1.0F, 1.0F, 1.0F, 1.0F});
    rect.z = visual.value("z", 0);
    return rect;
}

}  // namespace

void AssetManager::SetAssetRoot(std::filesystem::path root) {
    assetRoot_ = std::move(root);
}

std::filesystem::path AssetManager::Resolve(const std::filesystem::path& path) const {
    if (path.is_absolute()) {
        return path;
    }
    return std::filesystem::current_path() / assetRoot_ / path;
}

MapData AssetManager::LoadMap(const std::filesystem::path& path) const {
    std::ifstream stream(Resolve(path));
    if (!stream.is_open()) {
        throw std::runtime_error("Unable to open map file: " + path.string());
    }

    const auto json = nlohmann::json::parse(stream);

    MapData map;
    map.name = json.value("name", "unnamed");
    map.worldSize.x = json.at("world").value("width", 1280.0F);
    map.worldSize.y = json.at("world").value("height", 720.0F);
    map.spawnPoint.x = json.at("spawn").value("x", 64.0F);
    map.spawnPoint.y = json.at("spawn").value("y", 64.0F);
    map.spawnZ = json.at("spawn").value("z", 0);

    for (const auto& layerJson : json.value("layers", nlohmann::json::array())) {
        MapLayer layer;
        layer.name = layerJson.value("name", "unnamed");
        layer.drawOrder = ParseDrawOrder(layerJson.value("draw_order", "midground"));
        layer.z = layerJson.value("z", 0);
        for (const auto& visual : layerJson.value("visuals", nlohmann::json::array())) {
            layer.visuals.push_back(ParseVisualRect(visual));
        }
        map.layers.push_back(std::move(layer));
    }

    if (map.layers.empty()) {
        MapLayer fallbackLayer;
        fallbackLayer.name = "legacy_visuals";
        fallbackLayer.drawOrder = LayerDrawOrder::Midground;
        for (const auto& visual : json.value("visuals", nlohmann::json::array())) {
            fallbackLayer.visuals.push_back(ParseVisualRect(visual));
        }
        if (!fallbackLayer.visuals.empty()) {
            map.layers.push_back(std::move(fallbackLayer));
        }
    }

    for (const auto& blocker : json.value("blockers", nlohmann::json::array())) {
        math::Rect rect;
        rect.x = blocker.value("x", 0.0F);
        rect.y = blocker.value("y", 0.0F);
        rect.w = blocker.value("w", 0.0F);
        rect.h = blocker.value("h", 0.0F);
        map.blockers.push_back(rect);
    }

    for (const auto& marker : json.value("markers", nlohmann::json::array())) {
        MapMarker parsedMarker;
        parsedMarker.id = marker.value("id", "");
        parsedMarker.type = marker.value("type", "");
        parsedMarker.label = marker.value("label", "");
        parsedMarker.position.x = marker.value("x", 0.0F);
        parsedMarker.position.y = marker.value("y", 0.0F);
        parsedMarker.color = ParseColor(marker.value("color", nlohmann::json::array()), {1.0F, 1.0F, 1.0F, 1.0F});
        parsedMarker.z = marker.value("z", 0);
        map.markers.push_back(std::move(parsedMarker));
    }

    for (const auto& levelJson : json.value("levels", nlohmann::json::array())) {
        const auto levelZ = levelJson.value("z", 0);
        for (const auto& wallJson : levelJson.value("walls", nlohmann::json::array())) {
            math::Rect rect;
            rect.x = wallJson.value("x", 0.0F);
            rect.y = wallJson.value("y", 0.0F);
            rect.w = wallJson.value("w", 0.0F);
            rect.h = wallJson.value("h", 0.0F);
            map.blockers.push_back(rect);

            if (wallJson.contains("id")) {
                MapMarker wallMarker;
                wallMarker.id = wallJson.value("id", "");
                wallMarker.type = wallJson.value("type", "wall");
                wallMarker.label = wallJson.value("type", "wall");
                wallMarker.position = {rect.x, rect.y};
                wallMarker.color = {0.68F, 0.47F, 0.29F, 0.55F};
                wallMarker.z = levelZ;
                map.markers.push_back(std::move(wallMarker));
            }
        }
    }

    return map;
}

WorldPackageManifest AssetManager::LoadPackageManifestForMap(const std::filesystem::path& path) const {
    const auto resolvedMapPath = Resolve(path);
    const auto manifestPath = resolvedMapPath.parent_path().parent_path().parent_path() / "manifest.json";
    std::ifstream stream(manifestPath);
    if (!stream.is_open()) {
        return WorldPackageManifest {
            .mapId = resolvedMapPath.stem().string(),
            .packageVersion = "legacy-local",
            .contentHash = resolvedMapPath.string()
        };
    }

    const auto json = nlohmann::json::parse(stream);
    return WorldPackageManifest {
        .mapId = json.value("map_id", std::string {}),
        .packageVersion = json.value("package_version", std::string {}),
        .contentHash = json.value("content_hash", std::string {})
    };
}

}  // namespace ashpaw::engine::assets
