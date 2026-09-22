# shady

![Shady 3D desktop](screenshots/Screenshot2.png)

A small experimental **3D Wayland compositor** built on the TinyWL example from
wlroots 0.20.2.

Shady renders normal xdg-shell applications as textured 3D objects. Windows can
be moved through depth, tilted, deformed with a flexible wobble effect, picked
with 3D raycasts, and carried around while walking through the desktop in a
first-person camera.

The project is intentionally experimental: the goal is to explore what a
Wayland desktop feels like when windows are objects in a shared 3D space rather
than rectangles permanently attached to a 2D plane.

The imported TinyWL example is CC0; see [its license](LICENSES/tinywl-CC0.txt).

## Current features

- Custom GLES2 3D rendering pipeline for Wayland surfaces
- Perspective depth and per-window Z positioning
- Window thickness with directionally lit side faces
- Flexible 3D wobble/deformation
- Persistent per-window 3D rotation
- Orbit camera with pan and zoom
- First-person camera with WASD movement, mouse look, gravity, and jumping
- Center-ray window picking in first-person mode
- Grab, carry, rotate, and place windows in 3D space
- Adjustable grab distance with the scroll wheel
- FPS client-input mode so applications can receive normal keyboard input
- Horizontal reference floor and projected window shadows
- Animated crumple-style window closing
- Compositor-owned close snapshots for applications that show a confirmation dialog
- Editable GLSL under `shaders/`; rendering code lives under `src/render/`

## Controls

### First-person mode

| Input | Action |
|-------|--------|
| F2 | Enter / leave first-person mode |
| W / A / S / D | Move |
| Mouse | Look around |
| Space | Jump |
| Left click | Grab / release the window at the center of view |
| Scroll while holding a window | Move the held window closer / farther away |
| F3 | Toggle FPS controls / normal client keyboard input |

When a held window is released, its 3D position and rotation are preserved.

### Orbit mode

| Input | Action |
|-------|--------|
| Right-button drag | Orbit camera |
| Alt + middle-button drag | Pan camera |
| Alt + scroll wheel | Zoom |
| Alt + Shift + scroll wheel | Move the focused window along Z |
| Scroll wheel (no Alt) | Forward to the client |
| Middle-click (no Alt) | Forward to the client |
| Alt + arrows / WASD | Pan |
| Alt + Q / E | Orbit yaw |
| Alt + `=` / `-` | Zoom |
| Alt + `0` | Reset camera |
| Alt + F11 | Animate and request window close |
| F1 | Cycle windows |

Pointer interaction in orbit mode is raycast onto the transformed window
surfaces.

## NixOS development

Enable Nix flakes (`nix-command` and `flakes`) in your Nix configuration.
The committed `flake.lock` pins nixpkgs. The development shell provides the
compiler, Meson, Ninja, pkg-config, Wayland scanner/protocols, wlroots, graphics
libraries, and `foot`. No global `/usr/include` or `/usr/lib` installation
is needed.

1. Enter the development shell:

   ```sh
   nix develop
   pkg-config --modversion wlroots-0.20
   ```

   The current lock provides **wlroots 0.20.2**. Shady targets the wlroots 0.20
   API.

2. Configure and build:

   ```sh
   meson setup build
   ninja -C build
   ```

   For an existing build directory:

   ```sh
   meson setup --reconfigure build
   ```

3. Start Shady nested inside the current Wayland desktop:

   ```sh
   WLR_BACKENDS=wayland ./build/shady
   ```

   Shady requires the **GLES2** renderer for its custom shaders. Do not set
   `WLR_RENDERER=pixman`.

4. The compositor prints the Wayland socket it allocated:

   ```text
   Shady is listening on WAYLAND_DISPLAY=wayland-N
   Launch a client from another dev-shell terminal:
     WAYLAND_DISPLAY=wayland-N foot
   ```

   In another terminal, enter `nix develop` and launch applications using the
   printed display name. Keep the host `XDG_RUNTIME_DIR` unchanged.

   You can also start a client automatically:

   ```sh
   WLR_BACKENDS=wayland ./build/shady -s foot
   ```

After editing GLSL under `shaders/`, restart Shady. Shaders are loaded at
startup from the source-tree path baked in at configure time.

To inspect the wlroots pkg-config dependency closure:

```sh
pkg-config --cflags --libs --static wlroots-0.20
```

## Status

Shady is a research/experimental compositor rather than a production desktop.
The 3D interaction model is still evolving, and rendering, picking, physics,
multi-output behavior, and window orientation are active areas of development.
