#include "linuxpict/app.hpp"

#include "linuxpict/capture.hpp"
#include "linuxpict/render.hpp"

#include <gdk/gdkkeysyms.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace linuxpict {
namespace {

const char* tool_label(Tool tool) {
    switch (tool) {
        case Tool::Arrow: return "1 Arrow";
        case Tool::Box: return "2 Box";
        case Tool::Ellipse: return "3 Ellipse";
        case Tool::Line: return "4 Line";
        case Tool::Text: return "5 Text";
        case Tool::Crop: return "6 Crop";
    }
    return "";
}

const char* tool_icon(Tool tool) {
    switch (tool) {
        case Tool::Arrow: return "arrow";
        case Tool::Box: return "draw-rectangle";
        case Tool::Ellipse: return "draw-ellipse";
        case Tool::Line: return "draw-connector";
        case Tool::Text: return "draw-text";
        case Tool::Crop: return "crop-symbolic";
    }
    return "";
}

Glib::RefPtr<Gdk::Pixbuf> color_icon(double red, double green, double blue, int diameter = 14) {
    auto pixbuf = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, true, 8, diameter, diameter);
    const auto channel = [](double value) {
        return static_cast<unsigned>(std::round(std::clamp(value, 0.0, 1.0) * 255));
    };
    const auto rgba = (channel(red) << 24) | (channel(green) << 16) | (channel(blue) << 8) | 0xff;
    pixbuf->fill(rgba);
    return pixbuf;
}

std::string default_filename() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::ostringstream name;
    name << "LinuxPict-" << std::put_time(std::localtime(&time), "%Y%m%d-%H%M%S") << ".png";
    return name.str();
}

void show_error(Gtk::Window& parent, const std::string& title, const std::string& detail) {
    Gtk::MessageDialog dialog(parent, title, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE, true);
    dialog.set_secondary_text(detail);
    dialog.run();
}

}  // namespace

AnnotationCanvas::AnnotationCanvas(
    std::filesystem::path image_path,
    Document& document,
    sigc::slot<void> changed
) : image_path_(std::move(image_path)),
    image_(Gdk::Pixbuf::create_from_file(image_path_.string())),
    document_(document),
    changed_(std::move(changed)) {
    set_can_focus(true);
    add_events(Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::POINTER_MOTION_MASK);
}

void AnnotationCanvas::set_tool(Tool tool) {
    if (tool != Tool::Crop) {
        previous_tool_ = tool;
    }
    tool_ = tool;
    if (const auto window = get_window()) {
        const auto cursor = Gdk::Cursor::create(
            get_display(),
            tool == Tool::Text ? "text" : "crosshair"
        );
        window->set_cursor(cursor);
    }
}

void AnnotationCanvas::set_stroke_width(double width) {
    stroke_width_ = std::clamp(width, 2.0, 20.0);
}

void AnnotationCanvas::set_color(double red, double green, double blue) {
    red_ = red;
    green_ = green;
    blue_ = blue;
}

CanvasGeometry AnnotationCanvas::geometry() const {
    const auto allocation = get_allocation();
    return {
        document_.state().crop,
        static_cast<double>(allocation.get_width()),
        static_cast<double>(allocation.get_height()),
    };
}

bool AnnotationCanvas::on_draw(const Cairo::RefPtr<Cairo::Context>& context) {
    const auto allocation = get_allocation();
    context->set_source_rgb(0.15, 0.15, 0.17);
    context->rectangle(0, 0, allocation.get_width(), allocation.get_height());
    context->fill();

    const auto canvas = geometry();
    const auto display = canvas.display_rect();
    const auto crop = document_.state().crop;
    context->save();
    context->rectangle(display.x, display.y, display.width, display.height);
    context->clip();
    context->translate(display.x - crop.x * canvas.scale(), display.y - crop.y * canvas.scale());
    context->scale(canvas.scale(), canvas.scale());
    Gdk::Cairo::set_source_pixbuf(context, image_, 0, 0);
    context->paint();
    for (const auto& annotation : document_.state().annotations) {
        draw_annotation(context, annotation);
    }
    if (start_ && current_ && tool_ != Tool::Crop) {
        draw_annotation(context, {
            tool_, *start_, *current_, red_, green_, blue_, 1.0, stroke_width_, ""
        });
    }
    context->restore();

    context->set_line_width(1);
    context->set_source_rgba(1, 1, 1, 0.8);
    context->rectangle(display.x - 0.5, display.y - 0.5, display.width + 1, display.height + 1);
    context->stroke();

    if (start_ && current_ && tool_ == Tool::Crop) {
        const auto first = canvas.image_to_view(*start_);
        const auto second = canvas.image_to_view(*current_);
        const auto pending = Rect::between(first, second);
        context->save();
        context->set_fill_rule(Cairo::FILL_RULE_EVEN_ODD);
        context->set_source_rgba(0, 0, 0, 0.55);
        context->rectangle(display.x, display.y, display.width, display.height);
        context->rectangle(pending.x, pending.y, pending.width, pending.height);
        context->fill();
        context->restore();
        context->set_source_rgb(1, 1, 1);
        context->rectangle(pending.x, pending.y, pending.width, pending.height);
        context->stroke();
    }
    return true;
}

bool AnnotationCanvas::on_button_press_event(GdkEventButton* event) {
    if (event->button != 1) {
        return false;
    }
    grab_focus();
    start_ = geometry().view_to_image({event->x, event->y});
    current_ = start_;
    if (tool_ == Tool::Text) {
        add_text(*start_);
        start_.reset();
        current_.reset();
    }
    return true;
}

bool AnnotationCanvas::on_motion_notify_event(GdkEventMotion* event) {
    if (start_) {
        current_ = geometry().view_to_image({event->x, event->y});
        queue_draw();
    }
    return true;
}

bool AnnotationCanvas::on_button_release_event(GdkEventButton* event) {
    if (event->button != 1 || !start_) {
        return false;
    }
    current_ = geometry().view_to_image({event->x, event->y});
    if (std::abs(current_->x - start_->x) >= 3 || std::abs(current_->y - start_->y) >= 3) {
        if (tool_ == Tool::Crop) {
            document_.crop(Rect::between(*start_, *current_));
            set_tool(previous_tool_);
        } else {
            document_.add({
                tool_, *start_, *current_, red_, green_, blue_, 1.0, stroke_width_, ""
            });
        }
        changed_();
    }
    start_.reset();
    current_.reset();
    queue_draw();
    return true;
}

void AnnotationCanvas::add_text(Point point) {
    auto* parent = dynamic_cast<Gtk::Window*>(get_toplevel());
    Gtk::Dialog dialog("Add text", *parent, true);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Add", Gtk::RESPONSE_OK);
    Gtk::Entry entry;
    entry.set_placeholder_text("Annotation");
    entry.set_activates_default(true);
    dialog.get_content_area()->pack_start(entry, true, true, 12);
    dialog.set_default_response(Gtk::RESPONSE_OK);
    dialog.show_all();
    if (dialog.run() == Gtk::RESPONSE_OK && !entry.get_text().empty()) {
        document_.add({
            Tool::Text, point, point, red_, green_, blue_, 1.0, stroke_width_, entry.get_text()
        });
        changed_();
    }
}

AnnotationWindow::AnnotationWindow(
    const Glib::RefPtr<Gtk::Application>& application,
    std::filesystem::path image_path
) : Gtk::ApplicationWindow(application),
    image_path_(std::move(image_path)),
    document_(
        Gdk::Pixbuf::create_from_file(image_path_.string())->get_width(),
        Gdk::Pixbuf::create_from_file(image_path_.string())->get_height()
    ),
    canvas_(image_path_, document_, sigc::mem_fun(*this, &AnnotationWindow::update_title)) {
    set_default_size(1100, 760);
    set_position(Gtk::WIN_POS_CENTER);
    toolbar_.set_toolbar_style(Gtk::TOOLBAR_ICONS);

    Gtk::RadioToolButton::Group group;
    for (const auto tool : {Tool::Arrow, Tool::Box, Tool::Ellipse, Tool::Line, Tool::Text, Tool::Crop}) {
        auto* button = Gtk::manage(new Gtk::RadioToolButton(group));
        button->set_icon_name(tool_icon(tool));
        button->set_label(tool_label(tool));
        const auto help = std::string(tool_label(tool)) + " (" +
            std::string(1, static_cast<char>('1' + static_cast<int>(tool))) + ")";
        button->set_tooltip_text(help);
        button->get_accessible()->set_name(tool_label(tool));
        button->set_is_important(true);
        button->signal_toggled().connect([this, button, tool] {
            if (button->get_active()) {
                canvas_.set_tool(tool);
            }
        });
        toolbar_.append(*button);
        tool_buttons_[tool] = button;
    }
    toolbar_.append(*Gtk::manage(new Gtk::SeparatorToolItem()));

    Gtk::RadioToolButton::Group color_group;
    struct ColorOption { const char* name; double red; double green; double blue; };
    for (const auto& color : {
        ColorOption{"Red", 1.0, 0.20, 0.20},
        ColorOption{"Orange", 1.0, 0.58, 0.0},
        ColorOption{"Yellow", 1.0, 0.87, 0.0},
        ColorOption{"Green", 0.20, 0.78, 0.35},
        ColorOption{"Blue", 0.0, 0.48, 1.0},
        ColorOption{"Magenta", 1.0, 0.18, 0.80},
        ColorOption{"White", 1.0, 1.0, 1.0},
        ColorOption{"Black", 0.0, 0.0, 0.0},
    }) {
        auto* button = Gtk::manage(new Gtk::RadioToolButton(color_group));
        auto* image = Gtk::manage(new Gtk::Image(color_icon(color.red, color.green, color.blue)));
        button->set_icon_widget(*image);
        button->set_label(color.name);
        button->set_tooltip_text(color.name);
        button->get_accessible()->set_name(color.name);
        button->signal_toggled().connect([this, button, color] {
            if (button->get_active()) canvas_.set_color(color.red, color.green, color.blue);
        });
        toolbar_.append(*button);
        if (std::string(color.name) == "Red") button->set_active(true);
    }
    toolbar_.append(*Gtk::manage(new Gtk::SeparatorToolItem()));

    Gtk::RadioToolButton::Group size_group;
    for (const auto& [label, width] : {
        std::pair<const char*, double>{"•", 4.0},
        std::pair<const char*, double>{"●", 8.0},
        std::pair<const char*, double>{"⬤", 14.0},
    }) {
        auto* button = Gtk::manage(new Gtk::RadioToolButton(size_group, label));
        button->set_tooltip_text(
            std::string(width == 4 ? "Small" : width == 8 ? "Medium" : "Large") + " ([ / ])"
        );
        button->get_accessible()->set_name(width == 4 ? "Small" : width == 8 ? "Medium" : "Large");
        button->signal_toggled().connect([this, button, width] {
            if (button->get_active()) canvas_.set_stroke_width(width);
        });
        toolbar_.append(*button);
        if (width == 8.0) button->set_active(true);
    }
    toolbar_.append(*Gtk::manage(new Gtk::SeparatorToolItem()));

    const auto add_action = [this](
        const char* icon,
        const Glib::ustring& tooltip,
        const sigc::slot<void>& callback,
        const char* label = nullptr
    ) {
        auto* button = Gtk::manage(new Gtk::ToolButton());
        button->set_icon_name(icon);
        button->set_label(tooltip);
        button->set_tooltip_text(tooltip);
        button->get_accessible()->set_name(tooltip);
        if (label) {
            button->set_label(label);
            button->set_is_important(true);
        }
        button->signal_clicked().connect(callback);
        toolbar_.append(*button);
    };
    add_action("edit-undo-symbolic", "Undo (Ctrl+Z)", sigc::mem_fun(*this, &AnnotationWindow::undo));
    add_action("edit-redo-symbolic", "Redo (Ctrl+Shift+Z)", sigc::mem_fun(*this, &AnnotationWindow::redo));
    add_action("edit-delete-symbolic", "Clear all (Ctrl+Backspace)", sigc::mem_fun(*this, &AnnotationWindow::clear));
    toolbar_.append(*Gtk::manage(new Gtk::SeparatorToolItem()));

    auto* output_item = Gtk::manage(new Gtk::ToolItem());
    output_label_.set_tooltip_text("Size delivered to the agent");
    output_label_.set_margin_start(6);
    output_label_.set_margin_end(6);
    output_item->add(output_label_);
    toolbar_.append(*output_item);
    add_action(
        "view-fullscreen-symbolic",
        "Reset crop to the full image",
        sigc::mem_fun(*this, &AnnotationWindow::reset_crop)
    );
    toolbar_.append(*Gtk::manage(new Gtk::SeparatorToolItem()));
    add_action(
        "document-save-as-symbolic",
        "Save As… — write the PNG where you choose (Ctrl+S)",
        sigc::mem_fun(*this, &AnnotationWindow::save_as)
    );
    add_action(
        "folder-symbolic",
        "Copy PNG file path (Ctrl+Shift+Enter)",
        sigc::mem_fun(*this, &AnnotationWindow::copy_path)
    );
    add_action(
        "edit-copy-symbolic",
        "Copy annotated image (Ctrl+Enter)",
        sigc::mem_fun(*this, &AnnotationWindow::copy_image),
        "Copy"
    );
    add_action(
        "window-close-symbolic",
        "Close without copying (Esc)",
        sigc::mem_fun(*this, &AnnotationWindow::close_window)
    );

    layout_.pack_start(toolbar_, false, false);
    layout_.pack_start(canvas_, true, true);
    add(layout_);
    tool_buttons_.at(Tool::Crop)->set_active(true);
    update_title();
    show_all();
    present();
}

AnnotationWindow::~AnnotationWindow() {
    std::error_code error;
    std::filesystem::remove(image_path_, error);
}

void AnnotationWindow::select_tool(Tool tool) {
    tool_buttons_.at(tool)->set_active(true);
}

void AnnotationWindow::update_title() {
    const auto crop = document_.state().crop;
    output_label_.set_text(
        std::to_string(static_cast<int>(crop.width)) + " × " +
        std::to_string(static_cast<int>(crop.height)) + " px"
    );
    set_title(
        "LinuxPict — " + std::to_string(static_cast<int>(crop.width)) + " × " +
        std::to_string(static_cast<int>(crop.height)) + " px"
    );
    canvas_.queue_draw();
}

void AnnotationWindow::undo() {
    if (document_.undo()) update_title();
}

void AnnotationWindow::redo() {
    if (document_.redo()) update_title();
}

void AnnotationWindow::reset_crop() {
    document_.reset_crop();
    update_title();
}

void AnnotationWindow::clear() {
    document_.clear();
    update_title();
}

std::filesystem::path AnnotationWindow::render_temporary() const {
    const auto destination = temporary_png_path();
    render_png(image_path_, destination, document_.state().annotations, document_.state().crop);
    return destination;
}

void AnnotationWindow::copy_image() {
    try {
        const auto path = render_temporary();
        auto clipboard = Gtk::Clipboard::get();
        clipboard->set_image(Gdk::Pixbuf::create_from_file(path.string()));
        clipboard->store();
        close();
    } catch (const std::exception& error) {
        show_error(*this, "Could not copy image", error.what());
    }
}

void AnnotationWindow::copy_path() {
    try {
        const auto path = render_temporary();
        auto clipboard = Gtk::Clipboard::get();
        clipboard->set_text(path.string());
        clipboard->store();
        close();
    } catch (const std::exception& error) {
        show_error(*this, "Could not copy path", error.what());
    }
}

void AnnotationWindow::save_as() {
    Gtk::FileChooserDialog dialog(*this, "Save annotated screenshot", Gtk::FILE_CHOOSER_ACTION_SAVE);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Save", Gtk::RESPONSE_OK);
    dialog.set_do_overwrite_confirmation(true);
    dialog.set_current_name(default_filename());
    auto png_filter = Gtk::FileFilter::create();
    png_filter->set_name("PNG images");
    png_filter->add_mime_type("image/png");
    dialog.add_filter(png_filter);
    if (dialog.run() == Gtk::RESPONSE_OK) {
        try {
            render_png(
                image_path_,
                dialog.get_filename(),
                document_.state().annotations,
                document_.state().crop
            );
            close();
        } catch (const std::exception& error) {
            show_error(*this, "Could not save image", error.what());
        }
    }
}

void AnnotationWindow::close_window() {
    close();
}

bool AnnotationWindow::on_key_press_event(GdkEventKey* event) {
    const bool control = (event->state & GDK_CONTROL_MASK) != 0;
    const bool shift = (event->state & GDK_SHIFT_MASK) != 0;
    switch (event->keyval) {
        case GDK_KEY_1: select_tool(Tool::Arrow); return true;
        case GDK_KEY_2: select_tool(Tool::Box); return true;
        case GDK_KEY_3: select_tool(Tool::Ellipse); return true;
        case GDK_KEY_4: select_tool(Tool::Line); return true;
        case GDK_KEY_5: select_tool(Tool::Text); return true;
        case GDK_KEY_6: select_tool(Tool::Crop); return true;
        case GDK_KEY_c:
        case GDK_KEY_C:
            if (!control) {
                select_tool(Tool::Crop);
                return true;
            }
            break;
        case GDK_KEY_z:
        case GDK_KEY_Z:
            if (control) {
                shift ? redo() : undo();
                return true;
            }
            break;
        case GDK_KEY_s:
        case GDK_KEY_S:
            if (control) {
                save_as();
                return true;
            }
            break;
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
            if (control) {
                shift ? copy_path() : copy_image();
                return true;
            }
            break;
        case GDK_KEY_BackSpace:
            if (control) {
                document_.clear();
                update_title();
                return true;
            }
            break;
        case GDK_KEY_Escape:
            close();
            return true;
        case GDK_KEY_bracketleft:
            canvas_.set_stroke_width(canvas_.stroke_width() - 1);
            return true;
        case GDK_KEY_bracketright:
            canvas_.set_stroke_width(canvas_.stroke_width() + 1);
            return true;
        default:
            break;
    }
    return Gtk::ApplicationWindow::on_key_press_event(event);
}

LauncherWindow::LauncherWindow(const Glib::RefPtr<Gtk::Application>& application)
    : Gtk::ApplicationWindow(application), application_(application) {
    set_title("LinuxPict");
    set_default_size(420, 190);
    set_position(Gtk::WIN_POS_CENTER);
    layout_.set_margin_top(24);
    layout_.set_margin_bottom(24);
    layout_.set_margin_start(24);
    layout_.set_margin_end(24);
    title_.set_markup("<span size='x-large' weight='bold'>LinuxPict</span>");
    description_.set_text("Capture, crop, and annotate a screenshot for an AI agent.");
    description_.set_line_wrap(true);
    capture_button_.get_style_context()->add_class("suggested-action");
    capture_button_.signal_clicked().connect(sigc::mem_fun(*this, &LauncherWindow::capture));
    layout_.pack_start(title_, false, false);
    layout_.pack_start(description_, true, true);
    layout_.pack_start(capture_button_, false, false);
    add(layout_);
    show_all();
}

void LauncherWindow::capture() {
    hide();
    try {
        annotation_window_ = std::make_unique<AnnotationWindow>(application_, capture_with_portal());
        annotation_window_->signal_hide().connect([this] { close(); });
    } catch (const CaptureCancelled&) {
        show();
    } catch (const std::exception& error) {
        show();
        show_error(*this, "Could not capture the screen", error.what());
    }
}

}  // namespace linuxpict
