#pragma once

#include "linuxpict/model.hpp"

#include <gtkmm.h>

#include <filesystem>
#include <map>
#include <memory>
#include <optional>

namespace linuxpict {

class AnnotationCanvas final : public Gtk::DrawingArea {
public:
    AnnotationCanvas(std::filesystem::path image_path, Document& document, sigc::slot<void> changed);
    void set_tool(Tool tool);
    void set_stroke_width(double width);
    void set_color(double red, double green, double blue);
    [[nodiscard]] Tool tool() const { return tool_; }
    [[nodiscard]] double stroke_width() const { return stroke_width_; }

protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& context) override;
    bool on_button_press_event(GdkEventButton* event) override;
    bool on_motion_notify_event(GdkEventMotion* event) override;
    bool on_button_release_event(GdkEventButton* event) override;

private:
    [[nodiscard]] CanvasGeometry geometry() const;
    void add_text(Point point);

    std::filesystem::path image_path_;
    Glib::RefPtr<Gdk::Pixbuf> image_;
    Document& document_;
    sigc::slot<void> changed_;
    Tool tool_{Tool::Crop};
    Tool previous_tool_{Tool::Arrow};
    double stroke_width_{6.0};
    double red_{1.0};
    double green_{0.20};
    double blue_{0.20};
    std::optional<Point> start_;
    std::optional<Point> current_;
};

class AnnotationWindow final : public Gtk::ApplicationWindow {
public:
    AnnotationWindow(
        const Glib::RefPtr<Gtk::Application>& application,
        std::filesystem::path image_path
    );
    ~AnnotationWindow() override;

protected:
    bool on_key_press_event(GdkEventKey* event) override;

private:
    void select_tool(Tool tool);
    void update_title();
    void undo();
    void redo();
    void reset_crop();
    void clear();
    void copy_image();
    void copy_path();
    void save_as();
    void close_window();
    [[nodiscard]] std::filesystem::path render_temporary() const;

    std::filesystem::path image_path_;
    Document document_;
    Gtk::Box layout_{Gtk::ORIENTATION_VERTICAL};
    Gtk::Toolbar toolbar_;
    AnnotationCanvas canvas_;
    std::map<Tool, Gtk::RadioToolButton*> tool_buttons_;
    Gtk::Label output_label_;
};

class LauncherWindow final : public Gtk::ApplicationWindow {
public:
    explicit LauncherWindow(const Glib::RefPtr<Gtk::Application>& application);

private:
    void capture();

    Glib::RefPtr<Gtk::Application> application_;
    Gtk::Box layout_{Gtk::ORIENTATION_VERTICAL, 14};
    Gtk::Label title_;
    Gtk::Label description_;
    Gtk::Button capture_button_{"Capture screenshot", true};
    std::unique_ptr<AnnotationWindow> annotation_window_;
};

class LinuxPictApplication final : public Gtk::Application {
public:
    static Glib::RefPtr<LinuxPictApplication> create();

protected:
    LinuxPictApplication();
    int on_command_line(
        const Glib::RefPtr<Gio::ApplicationCommandLine>& command_line
    ) override;

private:
    void capture();

    bool background_held_{false};
    std::vector<std::unique_ptr<AnnotationWindow>> annotation_windows_;
    std::unique_ptr<LauncherWindow> launcher_;
};

}  // namespace linuxpict
