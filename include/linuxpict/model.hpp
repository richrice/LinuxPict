#pragma once

#include <optional>
#include <string>
#include <vector>

namespace linuxpict {

enum class Tool { Arrow, Box, Ellipse, Line, Text, Crop };

struct Point {
    double x{};
    double y{};
    bool operator==(const Point&) const = default;
};

struct Rect {
    double x{};
    double y{};
    double width{};
    double height{};

    [[nodiscard]] double right() const { return x + width; }
    [[nodiscard]] double bottom() const { return y + height; }
    [[nodiscard]] static Rect between(Point first, Point second);
    [[nodiscard]] Rect clamped_to(const Rect& bounds) const;
    bool operator==(const Rect&) const = default;
};

struct Annotation {
    Tool tool{Tool::Arrow};
    Point start;
    Point end;
    double red{1.0};
    double green{0.18};
    double blue{0.12};
    double alpha{1.0};
    double width{6.0};
    std::string text;
    bool operator==(const Annotation&) const = default;
};

struct DocumentState {
    std::vector<Annotation> annotations;
    Rect crop;
    bool operator==(const DocumentState&) const = default;
};

class Document {
public:
    Document(int width, int height);

    [[nodiscard]] const Rect& bounds() const { return bounds_; }
    [[nodiscard]] const DocumentState& state() const { return state_; }
    void add(Annotation annotation);
    bool crop(const Rect& rect);
    void reset_crop();
    void clear();
    bool undo();
    bool redo();

private:
    void commit(DocumentState next);

    Rect bounds_;
    DocumentState state_;
    std::vector<DocumentState> undo_;
    std::vector<DocumentState> redo_;
};

class CanvasGeometry {
public:
    CanvasGeometry(Rect crop, double view_width, double view_height, double padding = 18.0);
    [[nodiscard]] double scale() const;
    [[nodiscard]] Rect display_rect() const;
    [[nodiscard]] Point image_to_view(Point point) const;
    [[nodiscard]] Point view_to_image(Point point) const;

private:
    Rect crop_;
    double view_width_;
    double view_height_;
    double padding_;
};

}  // namespace linuxpict
