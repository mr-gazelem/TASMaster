# TASMaster

A Flipper Zero app that reads Tool-Assisted Speedrun (`.tasd`) files and replays 
controller inputs frame-accurately over USB, emulating a game controller on real hardware.

Currently supports the **Nintendo Switch Pro Controller** with more consoles planned.

---

## What is a TAS?

A Tool-Assisted Speedrun (TAS) is a recording of precise controller inputs, played back 
frame-by-frame to complete a game faster or more perfectly than humanly possible in 
real time. TASMaster lets you take those input files and replay them through your 
Flipper Zero as if it were a real controller.

---

## Requirements

- Flipper Zero with SD card
- [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) (Flipper build tool)
- A `.tasd` format input file — see [TASVideos](https://tasvideos.org) for examples
- USB cable

---

## Installation

**Clone the repo and build:**
```bash
git clone https://github.com/mr-gazelem/tasmaster.git
cd tasmaster
py -m ufbt
```

**Deploy to your Flipper:**
```bash
py -m ufbt launch
```

Or manually copy `dist/tasmaster.fap` to your Flipper's SD card under `apps/USB/`.

---

## How to Use

1. Copy your `.tasd` file to `/ext/tas/` on your Flipper's SD card
2. Connect your Flipper to your Nintendo Switch via USB
3. Open TASMaster from the apps menu
4. Press **OK** to browse for your file
5. Press **OK** again to begin playback

**Controls during playback:**

| Button | Action |
|--------|--------|
| OK | Pause / Resume |
| Back | Stop and return to file select |

---

## Planned Features

- [ ] Actual USB HID output to Nintendo Switch (in progress)
- [ ] PlayStation 4/5 controller support
- [ ] Xbox controller support
- [ ] `.tas` file format support
- [ ] Playback speed control
- [ ] Frame counter display
- [ ] Support for multiple input ports

---

## Project Structure
```
tasmaster/
├── tasmaster.c          # Main app entry point and UI
├── tasd_parser.c/h      # .tasd file format parser
├── hid_switch.h         # Switch Pro Controller HID descriptor
├── input_scheduler.c/h  # Timer-based frame playback engine
└── application.fam      # Flipper app manifest
```

---

## Contributing

Pull requests are welcome! If you want to add support for a new console, the best 
place to start is `hid_switch.h` — each console needs its own HID descriptor and 
button mapping. The `.tasd` format already includes a console type field so 
detection is handled automatically.

---

## License

MIT — see [LICENSE](LICENSE) for details.
