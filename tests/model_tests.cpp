#include "linuxpict/model.hpp"
#include "linuxpict/render.hpp"

#include <cairomm/surface.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>

using namespace linuxpict;

namespace {
int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}
}  // namespace

int main() {
    expect(Rect::between({9, 8}, {2, 3}) == Rect{2, 3, 7, 5}, "reverse drag normalizes");
    expect(Rect{-5, 3, 20, 20}.clamped_to({0, 0, 10, 10}) == Rect{0, 3, 10, 7},
           "rectangle clamps to bounds");

    Document document(100, 80);
    const Annotation annotation{Tool::Box, {2, 3}, {20, 30}, 1.0, 0.18, 0.12, 1.0, 6.0, ""};
    document.add(annotation);
    expect(document.crop({10.2, 8.8, 40.1, 30.1}), "valid crop accepted");
    expect(document.state().crop == Rect{10, 8, 41, 31}, "crop aligns outward to pixels");
    expect(document.undo() && document.state().crop == document.bounds(), "crop undo");
    expect(document.state().annotations == std::vector<Annotation>{annotation}, "annotation retained");
    expect(document.undo() && document.state().annotations.empty(), "annotation undo");
    expect(document.redo() && document.state().annotations.size() == 1, "annotation redo");

    Document nested(100, 80);
    nested.crop({10, 10, 50, 40});
    nested.crop({0, 20, 100, 50});
    expect(nested.state().crop == Rect{10, 20, 50, 30}, "nested crop is constrained");

    CanvasGeometry geometry({100, 50, 400, 200}, 1000, 700, 0);
    expect(geometry.scale() == 2.5, "geometry scale");
    expect(geometry.display_rect() == Rect{0, 100, 1000, 500}, "letterbox geometry");
    expect(geometry.view_to_image(geometry.image_to_view({300, 150})) == Point{300, 150},
           "geometry round trip");

    const auto temporary = std::filesystem::temp_directory_path() /
        ("LinuxPict-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()
        ));
    std::filesystem::create_directory(temporary);
    const auto source = temporary / "source.png";
    const auto output = temporary / "output.png";
    Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, 120, 90)->write_to_png(source.string());
    render_png(source, output, {annotation}, {10, 15, 80, 50});
    const auto rendered = Cairo::ImageSurface::create_from_png(output.string());
    expect(rendered->get_width() == 80 && rendered->get_height() == 50,
           "export preserves crop dimensions");
    std::filesystem::remove_all(temporary);

    if (failures == 0) {
        std::cout << "All model tests passed.\n";
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
