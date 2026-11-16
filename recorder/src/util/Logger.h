#pragma once
#include <iostream>
#include <string_view>

namespace tft::log {
inline void Info(std::string_view msg) { std::cerr << "[INFO] " << msg << std::endl; }
inline void Error(std::string_view msg) { std::cerr << "[ERROR] " << msg << std::endl; }
} // namespace tft::log
