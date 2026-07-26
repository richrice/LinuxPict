#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
bin_dir="$HOME/.local/bin"
desktop_dir="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
legacy_python_dir="${XDG_DATA_HOME:-$HOME/.local/share}/linuxpict"

if ! pkg-config --exists gtkmm-3.0; then
  echo "Missing gtkmm-3.0. On Ubuntu: sudo apt install libgtkmm-3.0-dev" >&2
  exit 1
fi

cmake -S "$repo_dir" -B "$repo_dir/build" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$repo_dir/build"
ctest --test-dir "$repo_dir/build" --output-on-failure

mkdir -p "$bin_dir" "$desktop_dir"
install -m 755 "$repo_dir/build/linuxpict" "$bin_dir/linuxpict"
if [[ -d "$legacy_python_dir/linuxpict" ]]; then
  rm -rf -- "$legacy_python_dir"
fi
chmod 755 "$bin_dir/linuxpict"
sed "s|@EXEC@|$bin_dir/linuxpict|g" "$repo_dir/data/com.github.richrice.LinuxPict.desktop.in" \
  > "$desktop_dir/com.github.richrice.LinuxPict.desktop"
chmod 644 "$desktop_dir/com.github.richrice.LinuxPict.desktop"

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
  gsettings set "$schema" binding "<Control><Alt>c"
  echo "Registered Ctrl+Alt+C as the GNOME capture shortcut."
fi

echo "Installed LinuxPict. Run: $bin_dir/linuxpict"
