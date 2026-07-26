# LinuxPict

LinuxPict is the Linux port of [MacPict](https://github.com/richrice/MacPict): a small desktop utility that shortens the “screenshot → annotate → hand it to an AI agent” loop.

Press **Ctrl+Alt+C**. LinuxPict asks the desktop for a screenshot, opens it at native resolution, and lets you crop or mark it up with arrows, boxes, ellipses, lines, and text. Copy the finished image, copy a temporary PNG path, or save it permanently.

![Linux](https://img.shields.io/badge/Linux-GNOME%20%7C%20X11%20%7C%20Wayland-blue)

## Requirements

- Python 3.10 or newer
- GTK 3, PyGObject, GdkPixbuf, and pycairo
- xdg-desktop-portal with a screenshot backend

Ubuntu 24.04 and current GNOME installations include these runtime components. LinuxPict has been built and run on Ubuntu 24.04 with GNOME 46 under Wayland.

## Install

```sh
./scripts/install.sh
```

This installs into `~/.local`, adds LinuxPict to the application launcher, and on GNOME registers **Ctrl+Alt+C** as a system-wide capture shortcut. Log out and back in if your shell does not notice a newly created `~/.local/bin`.

Run without installing:

```sh
./scripts/run.sh
./scripts/run.sh --capture
```

Build checks and tests:

```sh
./scripts/build.sh
./scripts/test.sh
```

## Using it

| Key | Action |
|---|---|
| `Ctrl+Alt+C` | launch the secure screenshot flow (GNOME install) |
| `1`…`5` | arrow / box / ellipse / line / text |
| `6` or `C` | crop |
| `Ctrl+Z` / `Ctrl+Shift+Z` | undo / redo, including crops |
| `Ctrl+Backspace` | clear annotations |
| `[` / `]` | smaller / larger stroke and text |
| `Ctrl+Enter` | copy annotated image and close |
| `Ctrl+Shift+Enter` | copy a temporary PNG path and close |
| `Ctrl+S` | save annotated PNG and close |
| `Esc` | close |

The first tool is Crop. After a crop, LinuxPict returns to the previous annotation tool. Exports retain the selected crop’s native pixel dimensions.

## Wayland capture

Wayland intentionally prevents applications from silently reading the display. LinuxPict uses the standard screenshot portal, so GNOME shows its trusted capture UI and asks what to share. This is the Linux security-model equivalent of MacPict’s macOS Screen Recording permission. On X11, the same portal-backed flow is used for consistent behavior.

## Project layout

- `linuxpict/` — GTK application, portal capture, document model, and Cairo renderer
- `tests/` — model, geometry, and native-resolution export tests
- `scripts/` — build, test, run, and per-user install commands
- `data/` — desktop launcher template
