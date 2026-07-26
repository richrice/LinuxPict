#pragma once

#include <filesystem>

namespace linuxpict {

class CaptureCancelled {};

std::filesystem::path capture_with_portal();
std::filesystem::path temporary_png_path();

}  // namespace linuxpict
