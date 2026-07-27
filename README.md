# LinuxPict

LinuxPict is the Linux port of [MacPict](https://github.com/richrice/MacPict): a small desktop utility that shortens the “screenshot → annotate → hand it to an AI agent” loop.

Press **Ctrl+Alt+C**. LinuxPict asks the desktop for a screenshot, opens it at native resolution, and lets you crop or mark it up with arrows, boxes, ellipses, lines, and text. Copy the finished image, copy its file path, or save it somewhere of your choosing. Copies are also written to your screenshots folder.

The annotation toolbar follows MacPict: icon-based modes, eight fixed export-safe colors, small/medium/large stroke and text sizes, undo/redo/clear, a live output-size readout, reset crop, Save As, copy-path, Copy, and close.

![Linux](https://img.shields.io/badge/Linux-GNOME%20%7C%20X11%20%7C%20Wayland-blue)

## Requirements

- A C++20 compiler and CMake 3.20 or newer
- gtkmm 3, Cairo, GLib/GIO, and Ninja
- xdg-desktop-portal with a screenshot backend

On Ubuntu:

```sh
sudo apt install build-essential cmake ninja-build libgtkmm-3.0-dev
```

LinuxPict is a compiled C++20/gtkmm application. It has been built and run on Ubuntu 24.04 with GNOME 46 under Wayland.

## Install

```sh
./scripts/install.sh
```

This installs into `~/.local`, adds LinuxPict to the application launcher, and on GNOME registers **Ctrl+Alt+C** as a system-wide capture shortcut if you have not already bound one. Log out and back in if your shell does not notice a newly created `~/.local/bin`.

There is no background daemon. Each hotkey press runs its own short-lived `linuxpict --capture` process that exits when you close its annotation window. This is deliberate: a capture forwarded to a long-lived instance is mapped by GNOME but not raised, so the window opens behind whatever currently has focus.

To change the shortcut on GNOME, open **Settings → Keyboard → View and Customize Shortcuts → Custom Shortcuts → LinuxPict Capture**. The configured command should remain `~/.local/bin/linuxpict --capture`.

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
| `Ctrl+Enter` | copy annotated image, saving a PNG to the screenshots folder |
| `Ctrl+Shift+Enter` | copy the path of that saved PNG |
| `Ctrl+S` | save annotated PNG and close |
| `Esc` | close |

The first tool is Crop. After a crop, LinuxPict returns to the previous annotation tool. Exports retain the selected crop’s native pixel dimensions.

## Clipboard lifetime

A clipboard selection on Wayland is served on demand by the process that owns it, and GNOME ships no clipboard manager that takes over once that process exits. Copying and quitting therefore leaves the clipboard empty.

So Copy and Copy path close the window but do **not** end the process: LinuxPict keeps running invisibly as the clipboard's owner, serving the image or path until you paste. It exits by itself as soon as another application copies something, and after 15 minutes regardless. Closing without copying exits immediately, as does Save As.

The one visible consequence is a `linuxpict` process outliving its window. That is deliberate; it is the only way a copied image survives on GNOME Wayland. Installing a clipboard manager such as GPaste would let the process exit right away, since gnome-shell would then hold the content itself.

## Where images are saved

Every Copy and Copy path also writes the annotated PNG next to GNOME's own screenshots as `LinuxPict-<date>-<time>.png`. A copy is never the only surviving artifact, so a missed paste costs nothing. Copy path hands out that file's path, so it stays valid indefinitely. Save As writes only where you choose.

Both halves of that location are localized the way GNOME localizes them, so the folder is `~/Pictures/Screenshots` in English but `~/Bilder/Bildschirmfotos` in German and `~/Изображения/Снимки экрана` in Russian:

- the parent comes from XDG user-dirs (`g_get_user_special_dir`), falling back to `~/Pictures` when user-dirs is not configured
- the subfolder is read from gnome-shell's own message catalog, which is where `_("Screenshots")` in its `screenshot.js` comes from; without gnome-shell installed it stays `Screenshots`

The timestamp in the filename is always ASCII digits regardless of locale, because the C++ global locale is left at `"C"`.

## Wayland capture

Wayland intentionally prevents applications from silently reading the display. LinuxPict uses the standard screenshot portal to obtain the image, then immediately opens its own crop and annotation window. GNOME may show a one-time access confirmation; it does not provide LinuxPict's editing interface. This is the Linux security-model equivalent of MacPict’s macOS Screen Recording permission. On X11, the same portal-backed flow is used for consistent behavior.

## Project layout

- `src/` and `include/` — C++ gtkmm application, portal capture, document model, and Cairo renderer
- `tests/` — compiled model and geometry tests
- `scripts/` — build, test, run, and per-user install commands
- `data/` — desktop launcher template
