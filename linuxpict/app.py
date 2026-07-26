from __future__ import annotations

import argparse
import os
import sys
import tempfile
from datetime import datetime
from pathlib import Path

import gi

gi.require_version("Gdk", "3.0")
gi.require_version("GdkPixbuf", "2.0")
gi.require_version("Gtk", "3.0")
from gi.repository import Gdk, GdkPixbuf, Gio, GLib, Gtk

from .capture import CaptureError, capture_with_portal
from .geometry import CanvasGeometry
from .model import Annotation, Document, Rect, Tool
from .render import draw_annotation, render_png


class AnnotationCanvas(Gtk.DrawingArea):
    def __init__(self, image_path: str, document: Document, changed):
        super().__init__()
        self.image_path = image_path
        self.image = GdkPixbuf.Pixbuf.new_from_file(image_path)
        self.document = document
        self.changed = changed
        self.tool = Tool.CROP
        self.previous_tool = Tool.ARROW
        self.stroke_width = 6.0
        self.start: tuple[float, float] | None = None
        self.current: tuple[float, float] | None = None
        self.add_events(Gdk.EventMask.BUTTON_PRESS_MASK | Gdk.EventMask.BUTTON_RELEASE_MASK | Gdk.EventMask.POINTER_MOTION_MASK)
        self.connect("draw", self.on_draw)
        self.connect("button-press-event", self.on_press)
        self.connect("motion-notify-event", self.on_motion)
        self.connect("button-release-event", self.on_release)

    @property
    def geometry(self) -> CanvasGeometry:
        allocation = self.get_allocation()
        return CanvasGeometry(self.document.state.crop, allocation.width, allocation.height)

    def set_tool(self, tool: Tool) -> None:
        if tool != Tool.CROP:
            self.previous_tool = tool
        self.tool = tool
        cursor_name = "text" if tool == Tool.TEXT else "crosshair"
        window = self.get_window()
        if window:
            window.set_cursor(Gdk.Cursor.new_from_name(window.get_display(), cursor_name))

    def on_draw(self, _widget, ctx):
        allocation = self.get_allocation()
        ctx.set_source_rgb(0.15, 0.15, 0.17)
        ctx.rectangle(0, 0, allocation.width, allocation.height)
        ctx.fill()
        geometry = self.geometry
        display = geometry.display_rect
        crop = self.document.state.crop
        ctx.save()
        ctx.rectangle(display.x, display.y, display.width, display.height)
        ctx.clip()
        ctx.translate(display.x - crop.x * geometry.scale, display.y - crop.y * geometry.scale)
        ctx.scale(geometry.scale, geometry.scale)
        Gdk.cairo_set_source_pixbuf(ctx, self.image, 0, 0)
        ctx.paint()
        for item in self.document.state.annotations:
            draw_annotation(ctx, item)
        if self.start and self.current and self.tool != Tool.CROP:
            draw_annotation(ctx, Annotation(self.tool, self.start, self.current, width=self.stroke_width))
        ctx.restore()
        ctx.set_line_width(1)
        ctx.set_source_rgba(1, 1, 1, 0.8)
        ctx.rectangle(display.x - 0.5, display.y - 0.5, display.width + 1, display.height + 1)
        ctx.stroke()
        if self.start and self.current and self.tool == Tool.CROP:
            a = geometry.image_to_view(*self.start)
            b = geometry.image_to_view(*self.current)
            pending = Rect.between(*a, *b)
            ctx.set_source_rgba(0, 0, 0, 0.55)
            ctx.rectangle(display.x, display.y, display.width, display.height)
            ctx.rectangle(pending.x, pending.y, pending.width, pending.height)
            ctx.set_fill_rule(1)
            ctx.fill()
            ctx.set_fill_rule(0)
            ctx.set_source_rgb(1, 1, 1)
            ctx.rectangle(pending.x, pending.y, pending.width, pending.height)
            ctx.stroke()
        return False

    def on_press(self, _widget, event):
        if event.button != 1:
            return False
        self.grab_focus()
        self.start = self.geometry.view_to_image(event.x, event.y)
        self.current = self.start
        if self.tool == Tool.TEXT:
            self._add_text(self.start)
            self.start = self.current = None
        return True

    def on_motion(self, _widget, event):
        if self.start:
            self.current = self.geometry.view_to_image(event.x, event.y)
            self.queue_draw()
        return True

    def on_release(self, _widget, event):
        if event.button != 1 or not self.start:
            return False
        self.current = self.geometry.view_to_image(event.x, event.y)
        if abs(self.current[0] - self.start[0]) >= 3 or abs(self.current[1] - self.start[1]) >= 3:
            if self.tool == Tool.CROP:
                self.document.crop(Rect.between(*self.start, *self.current))
                self.set_tool(self.previous_tool)
            else:
                self.document.add(Annotation(self.tool, self.start, self.current, width=self.stroke_width))
            self.changed()
        self.start = self.current = None
        self.queue_draw()
        return True

    def _add_text(self, point):
        dialog = Gtk.Dialog(title="Add text", transient_for=self.get_toplevel(), modal=True)
        dialog.add_buttons("_Cancel", Gtk.ResponseType.CANCEL, "_Add", Gtk.ResponseType.OK)
        entry = Gtk.Entry()
        entry.set_activates_default(True)
        entry.set_placeholder_text("Annotation")
        dialog.get_content_area().pack_start(entry, True, True, 12)
        dialog.set_default_response(Gtk.ResponseType.OK)
        dialog.show_all()
        if dialog.run() == Gtk.ResponseType.OK and entry.get_text().strip():
            self.document.add(Annotation(Tool.TEXT, point, point, width=self.stroke_width, text=entry.get_text().strip()))
            self.changed()
        dialog.destroy()
        self.queue_draw()


class AnnotationWindow(Gtk.ApplicationWindow):
    def __init__(self, application, image_path: str):
        super().__init__(application=application, title="LinuxPict")
        self.image_path = image_path
        pixbuf = GdkPixbuf.Pixbuf.new_from_file(image_path)
        self.document = Document(pixbuf.get_width(), pixbuf.get_height())
        self.set_default_size(1100, 760)
        self.set_position(Gtk.WindowPosition.CENTER)
        self.connect("key-press-event", self.on_key)
        layout = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        toolbar = Gtk.Toolbar()
        toolbar.set_style(Gtk.ToolbarStyle.BOTH_HORIZ)
        self.tool_buttons: dict[Tool, Gtk.ToggleToolButton] = {}
        for index, (tool, label) in enumerate([
            (Tool.ARROW, "Arrow"), (Tool.BOX, "Box"), (Tool.ELLIPSE, "Ellipse"),
            (Tool.LINE, "Line"), (Tool.TEXT, "Text"), (Tool.CROP, "Crop"),
        ], start=1):
            first_button = next(iter(self.tool_buttons.values()), None)
            button = (
                Gtk.RadioToolButton.new_from_widget(first_button)
                if first_button
                else Gtk.RadioToolButton.new(None)
            )
            button.set_label(f"{index} {label}")
            button.set_is_important(True)
            button.connect("toggled", self.on_tool, tool)
            toolbar.insert(button, -1)
            self.tool_buttons[tool] = button
        toolbar.insert(Gtk.SeparatorToolItem(), -1)
        for label, callback in [
            ("Undo", self.undo), ("Redo", self.redo), ("Reset crop", self.reset_crop),
            ("Copy", self.copy_image), ("Copy path", self.copy_path), ("Save As…", self.save_as),
        ]:
            button = Gtk.ToolButton.new(None, label)
            button.set_is_important(True)
            button.connect("clicked", lambda _b, cb=callback: cb())
            toolbar.insert(button, -1)
        self.canvas = AnnotationCanvas(image_path, self.document, self.update_title)
        self.canvas.set_can_focus(True)
        layout.pack_start(toolbar, False, False, 0)
        layout.pack_start(self.canvas, True, True, 0)
        self.add(layout)
        self.tool_buttons[Tool.CROP].set_active(True)
        self.update_title()
        self.show_all()
        self.present()

    def on_tool(self, button, tool):
        if button.get_active() and hasattr(self, "canvas"):
            self.canvas.set_tool(tool)

    def update_title(self):
        crop = self.document.state.crop
        self.set_title(f"LinuxPict — {int(crop.width)} × {int(crop.height)} px")
        self.canvas.queue_draw()

    def undo(self):
        if self.document.undo():
            self.update_title()

    def redo(self):
        if self.document.redo():
            self.update_title()

    def reset_crop(self):
        self.document.reset_crop()
        self.update_title()

    def _render_temp(self) -> str:
        fd, path = tempfile.mkstemp(prefix="LinuxPict-", suffix=".png")
        os.close(fd)
        render_png(self.image_path, path, self.document.state.annotations, self.document.state.crop)
        return path

    def copy_image(self):
        path = self._render_temp()
        clipboard = Gtk.Clipboard.get(Gdk.SELECTION_CLIPBOARD)
        clipboard.set_image(GdkPixbuf.Pixbuf.new_from_file(path))
        clipboard.store()
        self.destroy()

    def copy_path(self):
        path = self._render_temp()
        clipboard = Gtk.Clipboard.get(Gdk.SELECTION_CLIPBOARD)
        clipboard.set_text(path, -1)
        clipboard.store()
        self.destroy()

    def save_as(self):
        dialog = Gtk.FileChooserDialog(
            title="Save annotated screenshot", transient_for=self, action=Gtk.FileChooserAction.SAVE
        )
        dialog.add_buttons("_Cancel", Gtk.ResponseType.CANCEL, "_Save", Gtk.ResponseType.OK)
        dialog.set_do_overwrite_confirmation(True)
        dialog.set_current_name(datetime.now().strftime("LinuxPict-%Y%m%d-%H%M%S.png"))
        png_filter = Gtk.FileFilter()
        png_filter.set_name("PNG images")
        png_filter.add_mime_type("image/png")
        dialog.add_filter(png_filter)
        if dialog.run() == Gtk.ResponseType.OK:
            render_png(
                self.image_path,
                dialog.get_filename(),
                self.document.state.annotations,
                self.document.state.crop,
            )
            dialog.destroy()
            self.destroy()
            return
        dialog.destroy()

    def on_key(self, _widget, event):
        key = Gdk.keyval_name(event.keyval)
        ctrl = bool(event.state & Gdk.ModifierType.CONTROL_MASK)
        shift = bool(event.state & Gdk.ModifierType.SHIFT_MASK)
        mapping = {"1": Tool.ARROW, "2": Tool.BOX, "3": Tool.ELLIPSE, "4": Tool.LINE, "5": Tool.TEXT, "6": Tool.CROP, "c": Tool.CROP}
        if key.lower() in mapping and not ctrl:
            self.tool_buttons[mapping[key.lower()]].set_active(True)
            return True
        if ctrl and key.lower() == "z":
            self.redo() if shift else self.undo()
            return True
        if ctrl and key.lower() == "s":
            self.save_as()
            return True
        if ctrl and key == "Return":
            self.copy_path() if shift else self.copy_image()
            return True
        if ctrl and key == "BackSpace":
            self.document.clear()
            self.update_title()
            return True
        if key == "Escape":
            self.destroy()
            return True
        if key in ("bracketleft", "bracketright"):
            self.canvas.stroke_width = min(20, self.canvas.stroke_width + 1) if key == "bracketright" else max(2, self.canvas.stroke_width - 1)
            return True
        return False

    def do_destroy(self):
        try:
            Path(self.image_path).unlink(missing_ok=True)
        finally:
            Gtk.ApplicationWindow.do_destroy(self)


class LinuxPictApplication(Gtk.Application):
    def __init__(self):
        super().__init__(
            application_id="com.github.richrice.LinuxPict",
            flags=Gio.ApplicationFlags.HANDLES_COMMAND_LINE,
        )
        self.launcher = None

    def do_startup(self):
        Gtk.Application.do_startup(self)
        quit_action = Gio.SimpleAction.new("quit", None)
        quit_action.connect("activate", lambda *_: self.quit())
        self.add_action(quit_action)

    def do_activate(self):
        if self.launcher is None:
            self.launcher = Gtk.ApplicationWindow(application=self, title="LinuxPict")
            self.launcher.set_default_size(420, 190)
            self.launcher.set_position(Gtk.WindowPosition.CENTER)
            box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=14, margin=24)
            title = Gtk.Label()
            title.set_markup("<span size='x-large' weight='bold'>LinuxPict</span>")
            description = Gtk.Label(label="Capture, crop, and annotate a screenshot for an AI agent.")
            description.set_line_wrap(True)
            capture = Gtk.Button.new_with_mnemonic("_Capture screenshot")
            capture.get_style_context().add_class("suggested-action")
            capture.connect("clicked", lambda *_: self.capture())
            box.pack_start(title, False, False, 0)
            box.pack_start(description, True, True, 0)
            box.pack_start(capture, False, False, 0)
            self.launcher.add(box)
            self.launcher.show_all()
        self.launcher.present()

    def do_command_line(self, command_line):
        args = command_line.get_arguments()[1:]
        if "--capture" in args:
            self.activate()
            GLib.idle_add(self.capture)
        else:
            self.activate()
        return 0

    def capture(self):
        if self.launcher:
            self.launcher.hide()
        try:
            path = capture_with_portal()
            AnnotationWindow(self, path)
        except CaptureError as error:
            if "cancelled" not in str(error).lower():
                dialog = Gtk.MessageDialog(
                    transient_for=self.launcher,
                    modal=True,
                    message_type=Gtk.MessageType.ERROR,
                    buttons=Gtk.ButtonsType.CLOSE,
                    text="Could not capture the screen",
                )
                dialog.format_secondary_text(str(error))
                dialog.run()
                dialog.destroy()
            if self.launcher:
                self.launcher.show_all()
        except GLib.Error as error:
            dialog = Gtk.MessageDialog(
                transient_for=self.launcher,
                modal=True,
                message_type=Gtk.MessageType.ERROR,
                buttons=Gtk.ButtonsType.CLOSE,
                text="The desktop screenshot portal failed",
            )
            dialog.format_secondary_text(error.message)
            dialog.run()
            dialog.destroy()
            if self.launcher:
                self.launcher.show_all()
        return False


def main(argv=None) -> int:
    resolved_argv = list(sys.argv[1:] if argv is None else argv)
    parser = argparse.ArgumentParser(description="Capture and annotate screenshots")
    parser.add_argument("--capture", action="store_true", help="open the screenshot portal immediately")
    parser.parse_known_args(resolved_argv)
    app = LinuxPictApplication()
    passed = ["linuxpict", *resolved_argv]
    return app.run(passed)
