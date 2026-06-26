# kwin-clicktile

Mouse-driven WindowGrid/Snapdragin-style tiling for KWin on Wayland.

`kwin-clicktile` is a C++ KWin effect. While you are dragging a window by its
titlebar, right-click starts a tile selection overlay. Move across the monitor
grid, then release left click or right-click again to place the window into the
selected tile range.

## Build And Toggle

Use the root script for both install and uninstall:

```sh
./install_uninstall.sh
```

The script checks the current machine state. If `kwin-clicktile` is not
installed, it builds, installs, and asks KWin to load it. If it is already
installed, it unloads and removes it, restoring any files it backed up.

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
- Portrait monitors default to `1x3`
- Landscape monitors default to `2x2`
- Overlay grid, selection fill, and selection border colors

## Test

1. Drag a normal window by its titlebar with left mouse.
2. Press right mouse while still holding left to show the grid.
3. Move across the grid.
4. Release left mouse, or press right mouse again, to place the window.

Logs:

```sh
tail -f ~/.local/state/kwin-clicktile/effect/events.log
journalctl -b -f | grep -i kwin-clicktile
```

Emergency unload:

```sh
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin_clicktile
```

## Source Layout

- `src/effect.*`: effect lifecycle and KWin integration
- `src/input*`: global pointer observation and native drag tracking
- `src/tiles.cpp`: tile selection, output geometry, and final placement
- `src/overlay.cpp`: QuickScene overlay updates
- `src/settings.*`: KWin config keys, defaults, and monitor settings
- `src/log.cpp`: diagnostics
- `kcm/`: System Settings module
- `contents/ui/main.qml`: grid overlay

The Snapdragin reference clone is kept under `.tmp/snapdragin` when present.
