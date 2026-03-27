#include "ashpaw/engine/util/Log.hpp"

#include <memory>
#include <stdexcept>
#include <string>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace ashpaw::engine::util {

namespace {
std::shared_ptr<spdlog::logger> g_logger;
}

void InitializeLogger(std::string_view level) {
    if (!g_logger) {
        g_logger = spdlog::stdout_color_mt("ashpaw");
        g_logger->set_pattern("[%H:%M:%S] [%^%l%$] %v");
    }

    const auto levelString = std::string(level);
    if (levelString == "trace") {
        g_logger->set_level(spdlog::level::trace);
    } else if (levelString == "debug") {
        g_logger->set_level(spdlog::level::debug);
    } else if (levelString == "warn") {
        g_logger->set_level(spdlog::level::warn);
    } else if (levelString == "error") {
        g_logger->set_level(spdlog::level::err);
    } else {
        g_logger->set_level(spdlog::level::info);
    }
}

std::shared_ptr<spdlog::logger> Logger() {
    if (!g_logger) {
        throw std::runtime_error("Logger has not been initialized");
    }
    return g_logger;
}

}  // namespace ashpaw::engine::util
