#pragma once

#include "linuxpict/model.hpp"

#include <cairomm/context.h>
#include <filesystem>

namespace linuxpict {

void draw_annotation(const Cairo::RefPtr<Cairo::Context>& context, const Annotation& annotation);
void render_png(
    const std::filesystem::path& source,
    const std::filesystem::path& destination,
    const std::vector<Annotation>& annotations,
    const Rect& crop
);

}  // namespace linuxpict
