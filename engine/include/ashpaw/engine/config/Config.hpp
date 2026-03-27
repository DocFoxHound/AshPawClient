#pragma once

#include <filesystem>
#include <string>

namespace ashpaw::engine::config {

struct AppConfig {
    int windowWidth {1280};
    int windowHeight {720};
    bool fullscreen {false};
    bool vsync {true};
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
};

class Config {
public:
    static AppConfig LoadFromFile(const std::filesystem::path& path);
};

}  // namespace ashpaw::engine::config
