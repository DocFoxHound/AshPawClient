#pragma once

#include <memory>
#include <string_view>

#include <spdlog/logger.h>

namespace ashpaw::engine::util {

void InitializeLogger(std::string_view level);
std::shared_ptr<spdlog::logger> Logger();

}  // namespace ashpaw::engine::util
