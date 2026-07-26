#include "linuxpict/render.hpp"

#include <cairomm/surface.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace linuxpict {

void draw_annotation(const Cairo::RefPtr<Cairo::Context>& context, const Annotation& item) {
    context->save();
    context->set_source_rgba(item.red, item.green, item.blue, item.alpha);
    context->set_line_width(item.width);
    context->set_line_cap(Cairo::LINE_CAP_ROUND);
    context->set_line_join(Cairo::LINE_JOIN_ROUND);

    const auto x1 = item.start.x;
    const auto y1 = item.start.y;
    const auto x2 = item.end.x;
    const auto y2 = item.end.y;
    switch (item.tool) {
        case Tool::Line:
            context->move_to(x1, y1);
            context->line_to(x2, y2);
            context->stroke();
            break;
        case Tool::Arrow: {
            context->move_to(x1, y1);
            context->line_to(x2, y2);
            const auto angle = std::atan2(y2 - y1, x2 - x1);
            const auto head = std::max(14.0, item.width * 3);
            for (const auto offset : {-0.55, 0.55}) {
                context->move_to(x2, y2);
                context->line_to(
                    x2 - head * std::cos(angle + offset),
                    y2 - head * std::sin(angle + offset)
                );
            }
            context->stroke();
            break;
        }
        case Tool::Box: {
            const auto rect = Rect::between(item.start, item.end);
            context->rectangle(rect.x, rect.y, rect.width, rect.height);
            context->stroke();
            break;
        }
        case Tool::Ellipse: {
            const auto rect = Rect::between(item.start, item.end);
            context->translate(rect.x + rect.width / 2, rect.y + rect.height / 2);
            const auto radius_x = std::max(rect.width / 2, 0.001);
            const auto radius_y = std::max(rect.height / 2, 0.001);
            context->scale(radius_x, radius_y);
            context->arc(0, 0, 1, 0, std::numbers::pi * 2);
            context->set_line_width(item.width / std::max(radius_x, radius_y));
            context->stroke();
            break;
        }
        case Tool::Text:
            if (!item.text.empty()) {
                context->select_font_face("Sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
                const auto size = item.width <= 4 ? 24.0 : item.width <= 8 ? 36.0 : 56.0;
                context->set_font_size(size);
                std::size_t start = 0;
                int line = 1;
                while (start <= item.text.size()) {
                    const auto end = item.text.find('\n', start);
                    context->move_to(x1, y1 + size * line++);
                    context->show_text(item.text.substr(start, end - start));
                    if (end == std::string::npos) {
                        break;
                    }
                    start = end + 1;
                }
            }
            break;
        case Tool::Crop:
            break;
    }
    context->restore();
}

void render_png(
    const std::filesystem::path& source,
    const std::filesystem::path& destination,
    const std::vector<Annotation>& annotations,
    const Rect& crop
) {
    auto input = Cairo::ImageSurface::create_from_png(source.string());
    if (!input) {
        throw std::runtime_error("Could not decode captured PNG");
    }
    auto output = Cairo::ImageSurface::create(
        Cairo::FORMAT_ARGB32,
        static_cast<int>(crop.width),
        static_cast<int>(crop.height)
    );
    auto context = Cairo::Context::create(output);
    context->set_source(input, -crop.x, -crop.y);
    context->paint();
    context->translate(-crop.x, -crop.y);
    for (const auto& annotation : annotations) {
        draw_annotation(context, annotation);
    }
    output->write_to_png(destination.string());
}

}  // namespace linuxpict
