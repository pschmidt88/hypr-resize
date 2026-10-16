# hypr-resize

Resize tiled windows on Hyprland visually with the keyboard, using guiding lines.

This is a port of [`sway-resize`](https://github.com/moverest/sway-resize) by
moverest. The overlay, rendering and keyboard handling are unchanged; the Sway
IPC parts have been replaced with Hyprland ones.

## Usage

Bind `hypr-resize` to a key. When run, it shows an overlay over the focused
window's monitor:

Only horizontal resizing is supported: the overlay shows guiding lines for the
window's movable left or right edge. Pick one by its letter (`a`–`p`, from
-800px to +800px). `Escape` or `Backspace` cancels.

### Binding (Lua config)

```lua
hl.bind(mainMod .. " + SHIFT + R", hl.dsp.exec_cmd("hypr-resize"))
```

## How it works on Hyprland

Hyprland's layout tree is not exposed over IPC, and on the dwindle layout a
keyboard resize moves the split closest to the window, which can be either its
left or its right edge (same for top/bottom). Window geometry alone can't tell
which one.

So on startup, `hypr-resize` *probes* the window: in a single batch request it
resizes the window by 10px, reads its geometry back, and undoes the resize.
Since the whole batch is handled before Hyprland renders the next frame, the
probe is never visible. Whichever edge moved is the one offered in the overlay.

This works for the dwindle layout (with the default
`dwindle:smart_resizing = true`) and for floating windows. Other layouts
should work as long as a relative resize moves a single edge per axis.

## Building

Dependencies: `wayland`, `wayland-protocols`, `xkbcommon`, `cairo`, `jansson`.

```bash
make
make install            # installs to /usr/local/bin
make PREFIX=~/.local install
```

## License

GPL-3.0, like the original `sway-resize`.
