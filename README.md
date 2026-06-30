# kwin-clicktile

Mouse-driven grid tiling for KWin on Wayland.

`kwin-clicktile` is a C++ KWin effect. While you are dragging a window by its
titlebar, right-click starts a tile selection overlay. Move across the monitor
grid, then release left click or right-click again to place the window into the
selected tile range.

Takes inspiration from the Windows program WindowGrid.

<img width="500" alt="Demo" src="https://github.com/user-attachments/assets/131b0c99-64f8-408a-83e4-45169a9b7a3a" />
<img width="500" alt="Settings" src="https://github.com/user-attachments/assets/e2c4ebbd-a7a1-49fb-a5fc-2a256c4a358e" />

## Build And Toggle

You will need the following packages to run the build process on install. 

```bash
# Debian / Ubuntu / KDE Neon (Plasma 6)
sudo apt install cmake g++ extra-cmake-modules kwin-dev qt6-base-dev-tools libepoxy-dev

# Fedora (Plasma 6)
sudo dnf install cmake gcc-c++ extra-cmake-modules kwin-devel libepoxy-dev qt6-qtbase-private-devel

# Arch Linux (Plasma 6)
sudo pacman -S cmake extra-cmake-modules kwin qt6-base
```

Use the root script for both install and uninstall:

```sh
./install_uninstall.sh
```

The script checks the current machine state. If `kwin-clicktile` is not
installed, it builds, installs, and asks KWin to load it. If it is already
installed, it unloads KWin and removes the installed plugin files.

Explicit modes are also available:

```sh
./install_uninstall.sh --install
./install_uninstall.sh --uninstall
./install_uninstall.sh --status
```

## Configure

Open:

```text
System Settings > Window Management > Desktop Effects > kwin-clicktile
```

Settings:

- Per-monitor columns and rows
- Overlay grid, selection fill, and selection border colors

Emergency unload:

```sh
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin_clicktile
```

## Source Layout

- `src/effect.*`: effect lifecycle and KWin integration
- `src/input*`: global pointer observation and native drag tracking
- `src/tiles.cpp`: tile selection, output geometry, and final placement
- `src/overlay.cpp`: passive paint-screen grid overlay
- `src/settings.*`: KWin config keys, defaults, and monitor settings
- `kcm/`: System Settings module
