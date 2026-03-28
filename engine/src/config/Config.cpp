#include "ashpaw/engine/config/Config.hpp"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace ashpaw::engine::config {

AppConfig Config::LoadFromFile(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        throw std::runtime_error("Unable to open config file: " + path.string());
    }

    const auto json = nlohmann::json::parse(stream);
    AppConfig config;
    config.windowWidth = json.value("window_width", config.windowWidth);
    config.windowHeight = json.value("window_height", config.windowHeight);
    config.fullscreen = json.value("fullscreen", config.fullscreen);
    config.vsync = json.value("vsync", config.vsync);
    config.masterVolumePercent = json.value("master_volume_percent", config.masterVolumePercent);
    config.assetRoot = json.value("asset_root", config.assetRoot);
    config.mapPath = json.value("map_path", config.mapPath);
    config.serverHost = json.value("server_host", config.serverHost);
    config.serverPort = json.value("server_port", config.serverPort);
    config.playerName = json.value("player_name", config.playerName);
    config.autoConnect = json.value("auto_connect", config.autoConnect);
    config.connectTimeoutMs = json.value("connect_timeout_ms", config.connectTimeoutMs);
    config.handshakeTimeoutMs = json.value("handshake_timeout_ms", config.handshakeTimeoutMs);
    config.logLevel = json.value("log_level", config.logLevel);
    config.showDebugOverlay = json.value("show_debug_overlay", config.showDebugOverlay);
    config.showOnboardingHints = json.value("show_onboarding_hints", config.showOnboardingHints);
    config.cachedAuthoritativeName = json.value("cached_authoritative_name", config.cachedAuthoritativeName);
    config.cachedLastMap = json.value("cached_last_map", config.cachedLastMap);
    return config;
}

void Config::SaveToFile(const std::filesystem::path& path, const AppConfig& config) {
    const auto parentPath = path.parent_path();
    if (!parentPath.empty()) {
        std::filesystem::create_directories(parentPath);
    }

    std::ofstream stream(path);
    if (!stream.is_open()) {
        throw std::runtime_error("Unable to write config file: " + path.string());
    }

    const nlohmann::json json {
        {"window_width", config.windowWidth},
        {"window_height", config.windowHeight},
        {"fullscreen", config.fullscreen},
        {"vsync", config.vsync},
        {"master_volume_percent", config.masterVolumePercent},
        {"asset_root", config.assetRoot},
        {"map_path", config.mapPath},
        {"server_host", config.serverHost},
        {"server_port", config.serverPort},
        {"player_name", config.playerName},
        {"auto_connect", config.autoConnect},
        {"connect_timeout_ms", config.connectTimeoutMs},
        {"handshake_timeout_ms", config.handshakeTimeoutMs},
        {"log_level", config.logLevel},
        {"show_debug_overlay", config.showDebugOverlay},
        {"show_onboarding_hints", config.showOnboardingHints},
        {"cached_authoritative_name", config.cachedAuthoritativeName},
        {"cached_last_map", config.cachedLastMap}
    };
    stream << json.dump(2) << '\n';
}

}  // namespace ashpaw::engine::config
