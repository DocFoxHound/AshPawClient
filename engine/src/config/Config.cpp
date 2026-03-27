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
    return config;
}

}  // namespace ashpaw::engine::config
