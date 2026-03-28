#pragma once

#include <filesystem>
#include <string>

namespace ashpaw::engine::config {

struct AppConfig {
    int windowWidth {1280};
    int windowHeight {720};
    bool fullscreen {false};
    bool vsync {true};
    int masterVolumePercent {100};
    std::string assetRoot {"assets"};
    std::string mapPath {"maps/test_map.json"};
    std::string serverHost {"127.0.0.1"};
    int serverPort {7777};
    std::string playerName {"Local Prowler"};
    bool autoConnect {true};
    int connectTimeoutMs {2500};
    int handshakeTimeoutMs {2500};
    std::string logLevel {"info"};
    bool showDebugOverlay {true};
    bool showOnboardingHints {true};
    std::string cachedAuthoritativeName;
    std::string cachedLastMap;
};

class Config {
public:
    static AppConfig LoadFromFile(const std::filesystem::path& path);
    static void SaveToFile(const std::filesystem::path& path, const AppConfig& config);
};

}  // namespace ashpaw::engine::config
