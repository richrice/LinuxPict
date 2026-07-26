from __future__ import annotations

import os
import shutil
import tempfile
from pathlib import Path
from urllib.parse import unquote, urlparse

import gi

gi.require_version("Gio", "2.0")
from gi.repository import Gio, GLib


class CaptureError(RuntimeError):
    pass


def capture_with_portal(parent: str = "") -> str:
    """Capture through xdg-desktop-portal and return a private local PNG."""
    connection = Gio.bus_get_sync(Gio.BusType.SESSION, None)
    token = f"linuxpict{os.getpid()}"
    options = {
        "handle_token": GLib.Variant("s", token),
        # LinuxPict owns cropping and annotation. Asking the portal for an
        # "interactive" screenshot would instead open GNOME's capture editor.
        "interactive": GLib.Variant("b", False),
    }
    # The Request path is predictable from our unique bus name and token.
    # Subscribe before calling Screenshot so a fast non-interactive response
    # cannot race past the signal handler.
    sender = connection.get_unique_name().lstrip(":").replace(".", "_")
    expected_path = f"/org/freedesktop/portal/desktop/request/{sender}/{token}"
    loop = GLib.MainLoop()
    result: dict[str, object] = {}

    def response(_connection, _sender, _path, _interface, _signal, parameters):
        result["response"], result["values"] = parameters.unpack()
        loop.quit()

    subscription = connection.signal_subscribe(
        "org.freedesktop.portal.Desktop",
        "org.freedesktop.portal.Request",
        "Response",
        expected_path,
        None,
        Gio.DBusSignalFlags.NONE,
        response,
    )
    reply = connection.call_sync(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Screenshot",
        "Screenshot",
        GLib.Variant("(sa{sv})", (parent, options)),
        GLib.VariantType("(o)"),
        Gio.DBusCallFlags.NONE,
        -1,
        None,
    )
    request_path = reply.unpack()[0]
    if request_path != expected_path:
        connection.signal_unsubscribe(subscription)
        subscription = connection.signal_subscribe(
            "org.freedesktop.portal.Desktop",
            "org.freedesktop.portal.Request",
            "Response",
            request_path,
            None,
            Gio.DBusSignalFlags.NONE,
            response,
        )
    loop.run()
    connection.signal_unsubscribe(subscription)
    if result.get("response") != 0:
        raise CaptureError("Screen capture was cancelled")
    uri = result.get("values", {}).get("uri")
    if not uri:
        raise CaptureError("The desktop portal returned no image")
    parsed = urlparse(uri)
    if parsed.scheme != "file":
        raise CaptureError(f"Unsupported screenshot URI: {parsed.scheme}")
    source = unquote(parsed.path)
    fd, destination = tempfile.mkstemp(prefix="LinuxPict-", suffix=".png")
    os.close(fd)
    shutil.copyfile(source, destination)
    Path(destination).chmod(0o600)
    return destination
