# shady

<img src="\Screenshot.png">

A minimal Wayland compositor based on the TinyWL example from wlroots 0.20.2.
Supports xdg-shell windows, keyboard/pointer input, and interactive move/resize.
Alt+Escape exits; Alt+F1 cycles windows (the host desktop may intercept these).
The imported example is CC0; see [its license](LICENSES/tinywl-CC0.txt).

Windows are drawn with a custom GLES2 **3D** pipeline (textured quads +
orbit camera). Editable GLSL lives under `shaders/`; GL code is in
`src/render/`. A mild tint keeps the custom path visible.

### 3D camera controls

Windows stay on a flat desktop plane. You move the **camera** (default view is frontal / 2D-like).

| Input | Action |
|-------|--------|
| Right-button drag | Orbit camera |
| Alt + middle-button drag | Pan camera |
| Alt + scroll wheel | Zoom |
| Scroll wheel (no Alt) | Forwarded to the client |
| Middle-click (no Alt) | Forwarded to the client (e.g. paste) |
| Alt + arrows / WASD | Pan |
| Alt + Q / E | Orbit yaw |
| Alt + `=` / `-` | Zoom |
| Alt + `0` | Reset to frontal 2D view |

Pointer clicks are raycast onto the window surfaces.

## NixOS development

Enable Nix flakes (`nix-command` and `flakes`) in your Nix configuration.
The committed `flake.lock` pins nixpkgs. The shell supports x86_64-linux and
aarch64-linux and supplies the compiler, Meson, Ninja, pkg-config, Wayland
scanner/protocols, wlroots, graphics libraries, and `foot`. Its dependency set
follows the selected nixpkgs wlroots derivation, including enabled backends.
No global `/usr/include` or `/usr/lib` installation is needed.

1. From a terminal in your existing Wayland desktop session, enter the shell:

   ```sh
   nix develop
   pkg-config --modversion wlroots-0.20
   ```

   The current lock provides **wlroots 0.20.2**. The source targets the 0.20 API;
   review API changes before updating the lock to another wlroots series.

2. Configure and build:

   ```sh
   meson setup build
   ninja -C build
   ```

   For an existing build directory, use `meson setup --reconfigure build`.
   After switching toolchains or nixpkgs revisions, use `meson setup --wipe build`.

3. Start the compositor nested inside the host desktop:

   ```sh
   WLR_BACKENDS=wayland ./build/shady
   ```

   Custom shaders require the **GLES2** renderer. Do not set
   `WLR_RENDERER=pixman` (Shady will refuse to start). Nested testing needs a
   working GPU path under the host Wayland session.

   `./build/shady` also automatically selects a Wayland backend when the host
   Wayland environment is present. Preserve the host `WAYLAND_DISPLAY` and
   `XDG_RUNTIME_DIR`: the backend needs them to connect to the host. The dev
   shell does not override either variable. Do not set `WAYLAND_DISPLAY` to the
   new compositor's socket before starting it. Run as your desktop user.

4. Read the startup output, which reports the actual allocated socket:

   ```text
   Shady is listening on WAYLAND_DISPLAY=wayland-N
   Launch a client from another dev-shell terminal:
     WAYLAND_DISPLAY=wayland-N foot
   ```

   `wayland-N` here is a placeholder; the program prints the real name, which
   depends on sockets already in use. It is not necessarily `wayland-1`.

5. In another terminal in the same desktop session, enter `nix develop` in this
   repository and copy the printed `WAYLAND_DISPLAY=... foot` command. Leave
   `XDG_RUNTIME_DIR` unchanged and apply the display override only to the client.
   Alternatively, launch a client automatically with:

   ```sh
   WLR_BACKENDS=wayland ./build/shady -s foot
   ```

   Only the startup child receives Shady's display name; the compositor keeps
   the host environment. Stop Shady with Ctrl+C in its launching terminal.

After editing GLSL under `shaders/`, restart Shady (shaders are loaded at
startup from the source-tree path baked in at configure time).

Meson resolves headers and libraries through pkg-config from the shell. To
check the full wlroots pkg-config dependency closure:

```sh
pkg-config --cflags --libs --static wlroots-0.20
```
