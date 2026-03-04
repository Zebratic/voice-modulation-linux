# VML — Voice Modulation for Linux

Real-time voice effects processor that creates a virtual microphone through PipeWire. Apply pitch shifting, reverb, echo, bitcrushing, and more to your voice, then select "VML Virtual Microphone" in Discord, OBS, or any application.

![License](https://img.shields.io/badge/license-GPL--3.0-blue)

## Features

- **Virtual Microphone** — appears as a standard PipeWire audio source
- **7 Built-in Effects** — Gain, Noise Gate, Pitch Shift, Echo, Reverb, BitCrush
- **Effect Pipeline** — chain effects in any order, reorder with drag controls
- **Parameter Modulation** — automate any parameter with looping LFO-style sweeps (linear, ease in/out, rubberband, keyframe curves)
- **Audio Clip Recorder** — record 3 seconds of mic input, then loop it through your effect chain for hands-free testing
- **Profiles** — save/load effect chains with presets included (Deep Voice, Robot, Radio)
- **System Tray** — switch profiles, toggle processing, and monitor audio without opening the window
- **Low Latency** — ~5ms processing at 256 frames / 48kHz

## Quick Install

```bash
curl -fsSL https://raw.githubusercontent.com/USER/vml/main/scripts/install.sh | bash
```

Or clone and run locally:

```bash
git clone https://github.com/USER/vml.git
cd vml
bash scripts/install.sh
```

The install script detects your package manager (pacman / apt / dnf), installs dependencies, builds from source, and installs to `/usr/local`.

### Arch Linux (AUR)

```bash
yay -S vml
```

## Manual Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
```

### Dependencies

| Dependency | Arch | Debian/Ubuntu | Fedora |
|---|---|---|---|
| Qt6 | `qt6-base` | `qt6-base-dev` | `qt6-qtbase-devel` |
| PipeWire | `pipewire` | `libpipewire-0.3-dev` | `pipewire-devel` |
| CMake 3.20+ | `cmake` | `cmake` | `cmake` |
| C++20 compiler | `gcc` | `g++` | `gcc-c++` |

## Usage

1. Launch VML: `vml`
2. Pick effects from the list on the left and add them to the pipeline
3. Click an effect block to adjust its parameters with sliders
4. In your app (Discord, Zoom, OBS), select **VML Virtual Microphone** as the input device
5. Save your setup as a profile

### Parameter Modulation

Click the **~** button next to any slider to attach a modulator. Set a start value, end value, duration, and curve shape — the parameter will sweep back and forth continuously (ping-pong). Multiple modulators can run simultaneously on different parameters.

Curve types: Linear, Ease In/Out (smoothstep), Rubberband (fast attack), Keyframe (custom breakpoints).

### Clip Recorder

Click **Record 3s Clip** in the toolbar to capture mic input. Then enable **Loop Playback** to feed the recorded clip through your effect chain on repeat — useful for tweaking effects without talking.

### System Tray

Right-click the tray icon to switch profiles, enable/disable processing, or toggle audio monitor.

## Uninstall

```bash
bash scripts/install.sh --uninstall
```

Or if installed via AUR: `yay -R vml`

## License

GPL-3.0. See [LICENSE](LICENSE).
