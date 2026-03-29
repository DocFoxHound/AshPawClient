#include "ashpaw/engine/util/Log.hpp"

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

#include <SDL.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace ashpaw::engine::util {

namespace {
std::shared_ptr<spdlog::logger> g_logger;

spdlog::level::level_enum ParseLevel(std::string_view level) {
    if (level == "trace") {
        return spdlog::level::trace;
    }
    if (level == "debug") {
        return spdlog::level::debug;
    }
    if (level == "warn") {
        return spdlog::level::warn;
    }
    if (level == "error") {
        return spdlog::level::err;
    }
    return spdlog::level::info;
}

void SdlLogCallback(void*, int category, SDL_LogPriority priority, const char* message) {
    if (g_logger == nullptr || message == nullptr) {
        return;
    }

    auto level = spdlog::level::info;
    switch (priority) {
    case SDL_LOG_PRIORITY_VERBOSE:
        level = spdlog::level::trace;
        break;
    case SDL_LOG_PRIORITY_DEBUG:
        level = spdlog::level::debug;
        break;
    case SDL_LOG_PRIORITY_WARN:
        level = spdlog::level::warn;
        break;
    case SDL_LOG_PRIORITY_ERROR:
    case SDL_LOG_PRIORITY_CRITICAL:
        level = spdlog::level::err;
        break;
    case SDL_LOG_PRIORITY_INFO:
    default:
        level = spdlog::level::info;
        break;
    }

    g_logger->log(level, "SDL[cat={}] {}", category, message);
}
}

void InitializeLogger(std::string_view level) {
    if (!g_logger) {
        g_logger = spdlog::stdout_color_mt("ashpaw");
        g_logger->set_pattern("[%H:%M:%S] [%^%l%$] %v");
    }

    const auto parsedLevel = ParseLevel(level);
    g_logger->set_level(parsedLevel);
    g_logger->flush_on(parsedLevel);
    spdlog::set_default_logger(g_logger);

    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
    SDL_LogSetOutputFunction(SdlLogCallback, nullptr);
    g_logger->debug("Logger initialized at level '{}'", level);
}

std::shared_ptr<spdlog::logger> Logger() {
    if (!g_logger) {
        throw std::runtime_error("Logger has not been initialized");
    }
    return g_logger;
}

void LogEnvironmentVariable(std::string_view name) {
    const auto* value = std::getenv(std::string(name).c_str());
    Logger()->info("Env {}={}", name, value != nullptr ? value : "(unset)");
}

void LogSdlEnvironment() {
    Logger()->info("SDL platform diagnostics:");
    LogEnvironmentVariable("XDG_SESSION_TYPE");
    LogEnvironmentVariable("DISPLAY");
    LogEnvironmentVariable("WAYLAND_DISPLAY");
    LogEnvironmentVariable("SDL_VIDEODRIVER");
    LogEnvironmentVariable("SDL_AUDIODRIVER");
    LogEnvironmentVariable("PULSE_SERVER");
}

void LogSdlError(std::string_view context) {
    Logger()->error("{}: {}", context, SDL_GetError());
}

}  // namespace ashpaw::engine::util
