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

    // Emitted when the window is done for good: the user dismissed it, or the
    // clipboard it was serving has been taken over. Hiding is not enough to mean
    // this, because a copy leaves the window hidden but still alive.
    sigc::signal<void>& signal_finished() { return finished_; }

protected:
    bool on_key_press_event(GdkEventKey* event) override;
    bool on_delete_event(GdkEventAny* event) override;

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
    // Serves `targets` from this process until another application copies
    // something, which is the only way a Wayland selection can outlive a
    // visible window.
    void own_clipboard(
        const std::vector<Gtk::TargetEntry>& targets,
        const Gtk::Clipboard::SlotGet& get
    );
    void begin_lingering();
    void finish();
    // Writes the annotated PNG to ~/Pictures/Screenshots and returns its path.
    [[nodiscard]] std::filesystem::path render_backup() const;

    // Hiding a window unregisters it from its Gtk::Application, so the lingering
    // clipboard owner cannot ask get_application() for the reference it has to
    // release; it keeps its own.
    Glib::RefPtr<Gtk::Application> application_;
    std::filesystem::path image_path_;
    Document document_;
    Gtk::Box layout_{Gtk::ORIENTATION_VERTICAL};
    Gtk::Toolbar toolbar_;
    AnnotationCanvas canvas_;
    std::map<Tool, Gtk::RadioToolButton*> tool_buttons_;
    Gtk::Label output_label_;
    sigc::signal<void> finished_;
    sigc::connection linger_timeout_;
    // Replacing our own clipboard content also reports the old content as
    // cleared, so only the newest copy is allowed to end the wait.
    unsigned clipboard_generation_{0};
    bool lingering_{false};
    bool done_{false};
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
    void forget_window(AnnotationWindow* window);
    // Wraps `this` in a RefPtr that owns a reference. Glib::RefPtr's raw-pointer
    // constructor adopts without referencing, so RefPtr(this) would unreference
    // the application once per call.
    [[nodiscard]] Glib::RefPtr<Gtk::Application> self();

    std::vector<std::unique_ptr<AnnotationWindow>> annotation_windows_;
    std::unique_ptr<LauncherWindow> launcher_;
};

}  // namespace linuxpict
