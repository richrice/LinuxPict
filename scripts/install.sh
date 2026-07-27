#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
bin_dir="$HOME/.local/bin"
desktop_dir="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
autostart_dir="${XDG_CONFIG_HOME:-$HOME/.config}/autostart"
legacy_python_dir="${XDG_DATA_HOME:-$HOME/.local/share}/linuxpict"

if ! pkg-config --exists gtkmm-3.0; then
  echo "Missing gtkmm-3.0. On Ubuntu: sudo apt install libgtkmm-3.0-dev" >&2
  exit 1
fi

cmake -S "$repo_dir" -B "$repo_dir/build" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$repo_dir/build"
ctest --test-dir "$repo_dir/build" --output-on-failure

if pgrep -x linuxpict >/dev/null 2>&1; then
  pkill -TERM -x linuxpict
fi

mkdir -p "$bin_dir" "$desktop_dir"
# Older installs ran a --background daemon from autostart; captures forwarded to
# it opened behind the focused window. Each capture is its own process now.
rm -f "$autostart_dir/com.github.richrice.LinuxPict.desktop"
install -m 755 "$repo_dir/build/linuxpict" "$bin_dir/linuxpict"
if [[ -d "$legacy_python_dir/linuxpict" ]]; then
  rm -rf -- "$legacy_python_dir"
fi
chmod 755 "$bin_dir/linuxpict"
sed "s|@EXEC@|$bin_dir/linuxpict|g" "$repo_dir/data/com.github.richrice.LinuxPict.desktop.in" \
  > "$desktop_dir/com.github.richrice.LinuxPict.desktop"
chmod 644 "$desktop_dir/com.github.richrice.LinuxPict.desktop"
icon_dir="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor/scalable/actions"
mkdir -p "$icon_dir"
install -m 644 "$repo_dir/data/icons/linuxpict-arrow-symbolic.svg" "$icon_dir/"
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
  gtk-update-icon-cache -q -f -t \
    "${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor" >/dev/null 2>&1 || true
fi

if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database "$desktop_dir" >/dev/null 2>&1 || true
fi

if [[ "${XDG_CURRENT_DESKTOP:-}" == *GNOME* || "${XDG_CURRENT_DESKTOP:-}" == *gnome* ]]; then
  binding_path="/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/linuxpict/"
  current="$(gsettings get org.gnome.settings-daemon.plugins.media-keys custom-keybindings)"
  if [[ "$current" != *"$binding_path"* ]]; then
    if [[ "$current" == "@as []" || "$current" == "[]" ]]; then
      updated="['$binding_path']"
    else
      updated="${current%]} , '$binding_path']"
    fi
    gsettings set org.gnome.settings-daemon.plugins.media-keys custom-keybindings "$updated"
  fi
  schema="org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:$binding_path"
  gsettings set "$schema" name "LinuxPict Capture"
  gsettings set "$schema" command "$bin_dir/linuxpict --capture"
  # Only claim a key when none is set, so reinstalling keeps a shortcut the
  # user rebound by hand.
  if [[ "$(gsettings get "$schema" binding)" == "''" ]]; then
    gsettings set "$schema" binding "<Control><Alt>c"
    echo "Registered Ctrl+Alt+C as the GNOME capture shortcut."
  else
    echo "Kept the existing GNOME capture shortcut: $(gsettings get "$schema" binding)"
  fi

  # gsd-media-keys runs the hotkey command inside an app-gnome-linuxpict-*.scope,
  # so the screenshot portal sees the app id "linuxpict". Without a stored grant
  # it tries to prompt, and GNOME refuses to show an access dialog for an app
  # that is not focused -- the capture is denied and nothing appears. Record the
  # grant up front. Revoke with:
  #   gdbus call --session --dest org.freedesktop.impl.portal.PermissionStore \
  #     --object-path /org/freedesktop/impl/portal/PermissionStore \
  #     --method org.freedesktop.impl.portal.PermissionStore.DeletePermission \
  #     screenshot screenshot linuxpict
  if command -v gdbus >/dev/null 2>&1; then
    if gdbus call --session --dest org.freedesktop.impl.portal.PermissionStore \
      --object-path /org/freedesktop/impl/portal/PermissionStore \
      --method org.freedesktop.impl.portal.PermissionStore.SetPermission \
      screenshot true screenshot "$(basename "$bin_dir/linuxpict")" '["yes"]' \
      >/dev/null 2>&1; then
      echo "Granted the screenshot portal permission for hotkey captures."
    else
      echo "Could not preset the screenshot portal permission; the first hotkey" >&2
      echo "capture may be denied. Run LinuxPict once from the launcher." >&2
    fi
  fi
fi

echo "Installed LinuxPict. Run: $bin_dir/linuxpict"
