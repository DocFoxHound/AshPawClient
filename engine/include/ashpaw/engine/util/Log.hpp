#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <spdlog/logger.h>

namespace ashpaw::engine::util {

void InitializeLogger(std::string_view level);
std::shared_ptr<spdlog::logger> Logger();
void LogEnvironmentVariable(std::string_view name);
void LogSdlEnvironment();
void LogSdlError(std::string_view context);

}  // namespace ashpaw::engine::util
