#include "linuxpict/model.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace linuxpict {

Rect Rect::between(Point first, Point second) {
    return {
        std::min(first.x, second.x),
        std::min(first.y, second.y),
        std::abs(second.x - first.x),
        std::abs(second.y - first.y),
    };
}

Rect Rect::clamped_to(const Rect& bounds) const {
    const auto left = std::max(x, bounds.x);
    const auto top = std::max(y, bounds.y);
    const auto clipped_right = std::min(right(), bounds.right());
    const auto clipped_bottom = std::min(bottom(), bounds.bottom());
    return {left, top, std::max(0.0, clipped_right - left), std::max(0.0, clipped_bottom - top)};
}

Document::Document(int width, int height)
    : bounds_{0, 0, static_cast<double>(width), static_cast<double>(height)},
      state_{{}, bounds_} {}

void Document::commit(DocumentState next) {
    if (next == state_) {
        return;
    }
    undo_.push_back(state_);
    state_ = std::move(next);
    redo_.clear();
}

void Document::add(Annotation annotation) {
    auto next = state_;
    next.annotations.push_back(std::move(annotation));
    commit(std::move(next));
}

bool Document::crop(const Rect& rect) {
    const auto clipped = rect.clamped_to(state_.crop);
    if (clipped.width < 16 || clipped.height < 16) {
        return false;
    }
    auto next = state_;
    const auto left = std::floor(clipped.x);
    const auto top = std::floor(clipped.y);
    next.crop = {left, top, std::ceil(clipped.right()) - left, std::ceil(clipped.bottom()) - top};
    commit(std::move(next));
    return true;
}

void Document::reset_crop() {
    auto next = state_;
    next.crop = bounds_;
    commit(std::move(next));
}

void Document::clear() {
    auto next = state_;
    next.annotations.clear();
    commit(std::move(next));
}

bool Document::undo() {
    if (undo_.empty()) {
        return false;
    }
    redo_.push_back(state_);
    state_ = std::move(undo_.back());
    undo_.pop_back();
    return true;
}

bool Document::redo() {
    if (redo_.empty()) {
        return false;
    }
    undo_.push_back(state_);
    state_ = std::move(redo_.back());
    redo_.pop_back();
    return true;
}

CanvasGeometry::CanvasGeometry(Rect crop, double view_width, double view_height, double padding)
    : crop_(crop), view_width_(view_width), view_height_(view_height), padding_(padding) {}

double CanvasGeometry::scale() const {
    const auto available_width = std::max(1.0, view_width_ - padding_ * 2);
    const auto available_height = std::max(1.0, view_height_ - padding_ * 2);
    return std::min(available_width / crop_.width, available_height / crop_.height);
}

Rect CanvasGeometry::display_rect() const {
    const auto width = crop_.width * scale();
    const auto height = crop_.height * scale();
    return {(view_width_ - width) / 2, (view_height_ - height) / 2, width, height};
}

Point CanvasGeometry::image_to_view(Point point) const {
    const auto display = display_rect();
    return {
        display.x + (point.x - crop_.x) * scale(),
        display.y + (point.y - crop_.y) * scale(),
    };
}

Point CanvasGeometry::view_to_image(Point point) const {
    const auto display = display_rect();
    return {
        std::clamp(crop_.x + (point.x - display.x) / scale(), crop_.x, crop_.right()),
        std::clamp(crop_.y + (point.y - display.y) / scale(), crop_.y, crop_.bottom()),
    };
}

}  // namespace linuxpict
