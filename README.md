# Flux

Native Linux settings app for the **LAMZU INCA 8K** mouse and the **MADLIONS MAD LIGHT 60 HE** Hall-effect keyboard. It replaces the vendor's Windows app (LAMZU Aurora) and the WebHID page (hub.fgg.com.cn). Built with Qt 6 / QML, dark and Apple-style, with springy animations.

## Install (CachyOS / Arch)

```bash
cd packaging/arch && makepkg -si
```

This installs `flux`, `fluxctl`, the desktop entry and the udev rule (`/usr/lib/udev/rules.d/70-flux.rules`). The rule gives the logged-in user access to `37b0:*` (LAMZU) and `373b:*` (MADLIONS) hidraw nodes. Replug the devices, or run `sudo udevadm trigger --subsystem-match=hidraw`.

To run from the build tree without installing:

```bash
cmake -S . -B build -G Ninja && cmake --build build && ./build/flux
```

Other ways to start it:
- `flux --demo` shows simulated devices with no hardware access.
- `flux --hidden` starts in the tray.

## Features

**Mouse (LAMZU INCA 8K, receiver or cable)**
- 1–5 DPI stages (50–30000), separate X/Y, per-stage LED colour, active stage
- Polling rate up to 8 kHz, lift-off distance 0.7 / 1 / 2 mm
- Motion Sync, angle snapping, angle tune (±30°), ripple control
- Competition mode, 20K FPS (firmware ≥ 0.0.0.16), debounce
- Remap all 6 buttons: clicks, keys and shortcuts, media, DPI cycle/up/down/lock, rapid fire, macros, polling/LOD cycles, disable
- Macro recorder; up to 30 macros stored on the mouse
- 3 on-board profiles, battery %, sleep timer, factory reset
- Tray icon with battery level and low-battery notifications

**Keyboard (MAD LIGHT 60 HE)**
- Per-key actuation (0.10–3.50 mm), Rapid Trigger, top/bottom dead zones
- Live key travel view and calibration
- SOCD / Snap Tap / Rappy Snappy pairs, Dynamic Keystroke, Mod-Tap, Toggle
- Keymap editor for all 4 layers, with "press a key to bind"
- Macros (VIA dynamic macros)
- Lighting: built-in effects, per-key paint, side light
- **Flux Effects**: 29 host-rendered per-key animations streamed live
  - Reactive (use the analog depth of each press): Ripples, Flutter, Shockwave, Beams, Splash, Fireworks, Magnetic Field, Water (wave physics), Lightning (Tesla arcs between held keys), Ink (curl-noise flow), Constellation, Life, Tempo (learns your tapping BPM), Echo, Depth Glow, Heatmap.
  - Audio (from what the PC is playing, via `parec`): Sound Wave, Spectrum, Bass Drop, Audio Aura.
  - Ambient: Aurora, Plasma, Ocean, Ember, Neon Rain, Starlight, Spectrum Spin, Gradient Drift, Breathe.
  - "Bake" any frame to the keyboard so it persists without Flux running.
- WASD swap, Win lock, NKRO, Mac mode, competitive mode, bottom-out trigger

**Profiles**
- Save and restore full device snapshots as JSON in `~/.local/share/flux/profiles/`
- One-click **osu!** presets for keyboard (A D / 4K / 7K) and mouse

## CLI

```
fluxctl list
fluxctl mouse info | battery | dpi | set-dpi <stage> <dpi> | polling <hz> | lod <mm> | profile <n>
fluxctl kb info | actuation | rt | keymap | travel
```

## Protocols

These were reverse-engineered from the vendors' own apps by static analysis:
- [docs/protocol-lamzu-inca.md](docs/protocol-lamzu-inca.md): 64-byte feature reports on the `0xFFFF` interface, with events on input report 4
- [docs/protocol-mad-light-60-he.md](docs/protocol-mad-light-60-he.md): QMK VIA raw HID (`0xFF60`) plus vendor commands under `0x96`

Flux never sends bootloader or firmware-update commands.

## Licence

MIT. Inter font (OFL 1.1) and Lucide icons (ISC) are bundled with their licences. Flux is not affiliated with LAMZU or MADLIONS / FGG.
