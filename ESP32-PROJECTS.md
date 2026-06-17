# ESP32-S3 Projects

Hardware baseline: ESP32-S3 N16R8 (16MB Flash, 8MB OPI PSRAM, dual-core 240MHz, native USB, WiFi, BLE).
Headless-first — web UI via phone is the primary interface. No display assumed unless noted.

---

## Z-Machine Text Adventure Server

**Status:** Idea — not started  
**Extra hardware:** None

Play classic Infocom games (Zork, Hitchhiker's Guide, Planetfall, etc.) in a phone browser over WiFi.
The ESP32 runs a Z-machine interpreter; the browser is a dumb terminal connected via WebSocket.

### Why This One First

- No extra hardware — just the board and a USB cable
- 8MB PSRAM comfortably holds Z-machine state for .z5/.z8 files
- 16MB Flash holds several story files simultaneously in LittleFS
- MicroZork example in this repo is already a foundation to build on
- Frotz has been ported to ESP32 — proven path

### Architecture

```
Phone browser  ←→  WebSocket  ←→  ESP32-S3
                                  ├── WebServer (serves index.html)
                                  ├── WebSocket handler (forwards I/O)
                                  ├── Z-machine interpreter (frotz or equivalent)
                                  └── LittleFS (story .z5/.z8 files)
```

- **Core 0:** WiFi stack + WebServer + WebSocket handler
- **Core 1:** Z-machine interpreter loop (blocking reads from WebSocket input queue)
- **mDNS:** advertise as `zork.local` so no IP hunting on phone
- **Input queue:** ring buffer between WebSocket ISR and interpreter core
- **Output:** interpreter writes → WebSocket broadcast → browser appends to terminal div

### Web UI

Single-page terminal emulator:
- Monospace font, dark background, scrollback buffer
- Input line at bottom, full-width, large touch keyboard friendly
- Game selector on first load (if multiple .z5 files present in LittleFS)
- "Restart" button resets interpreter state without reflashing

### Story File Management

- Upload via `/upload` endpoint (multipart form, drag-and-drop on desktop)
- List via `/files` JSON endpoint
- LittleFS with 16M Flash → plenty of room for a full Infocom back-catalog
- File names become the game selector labels (strip `.z5`/`.z8` extension)

### Milestones

1. **Bringup:** Serve static index.html + echo WebSocket (no interpreter yet) — confirm phone terminal works
2. **Interpreter:** Integrate frotz ESP32 port, wire stdio to WebSocket queues, load a single hardcoded .z5
3. **Game selector:** List LittleFS files, let user pick on load
4. **File upload:** `/upload` endpoint, manage story library from browser
5. **Polish:** mDNS, restart button, scrollback limit, save/restore game state to LittleFS

### Interpreter: dumb-frotz (dfrotz)

Use the `DUMB` build target from the canonical frotz source (gitlab.com/DavidGriffith/frotz).

**Why dumb-frotz over alternatives:**
- AZIP_ESP32 (talofer99) is tied to VGA+PS2 via fabGL — wrong I/O model entirely
- dumb-frotz has zero dependencies beyond stdlib; I/O is two files (`dumb_input.c`, `dumb_output.c`)
- Supports z1–z8, covering the full Infocom catalog
- The `os_*` abstraction layer means porting = replacing ~2 files, not rewriting the interpreter

**Porting surface (what we actually change):**
- `dumb_input.c` → reads from a FreeRTOS queue populated by the WebSocket handler on core 0
- `dumb_output.c` → writes to a WebSocket broadcast buffer
- File I/O in `common/files.c` → redirect `fopen`/`fread` to LittleFS calls
- Large allocations → `ps_malloc()` to use the 8MB PSRAM for story file data

**Memory model:** Z-machine story files are 256KB (.z5) to 512KB (.z8). Load entirely into PSRAM. Interpreter stack/state is a few KB in normal heap. No pressure on either.

### Decisions

- **Save states:** not in v1. Restart resets the interpreter from scratch.
- **Sessions:** single player. First WebSocket connection owns the game; others are ignored or shown a "busy" message.
- **Build layout:** two separate directories in the PlatformIO project, matching the ESP32 Clicker pattern:
  - `lib/frotz/` — frotz C sources (interpreter core + our custom I/O files). PlatformIO auto-discovers libraries in `lib/`.
  - `data/` — story files (`.z5`, `.z8`) uploaded to LittleFS via `flash fs`. Same mechanism as `data/index.html` in the Clicker.

---

## Seeds

Ideas to revisit later. One-liners for now — expand when ready to start.

### HID / Gaming
- **Hold Mode v2** — encoder wheel controls hold duration in real-time while macro runs; dial up/down cast speed without stopping
- **Subnautica Resource Farmer** — multi-key sequence with configurable timing per loop; web UI shows current "farm program"
- **Controller Remap Proxy** — USB HID gamepad passthrough with macro/remap layer between controller and PC
- **Spellbook** — web UI with named presets ("Skyrim Mage", "Subnautica Dive"), one tap to switch macro profiles

### Ham Radio
- **CW Keyer** — iambic paddle → Morse output via audio tone or direct key line; speed/sidetone via web UI
- **APRS iGate** — receives APRS packets on 144.390 MHz (SA818 module), gates to aprs.fi over WiFi
- **Beacon Transmitter** — periodic CW or WSPR beacon using Si5351 as RF source
- **Rig Control** — CAT protocol bridge: web UI → serial → radio; phone becomes rig controller
- **Frequency Counter** — input signal → ESP32 frequency measurement → web display; calibrate against GPS 1PPS
- **Voice Keyer** — records short audio clips (CQ calls, contest exchanges) to PSRAM via I2S mic; replays on button press via I2S amp; web UI manages clip library; PTT line keys the rig; extra hardware: I2S mic (INMP441), I2S amp (MAX98357A), PTT relay, audio jack

### Home Automation
- **Cat Box Appliance** — stepper + web UI; add schedule/timer so it runs autonomously (see project_catbox_pcb.md)
- **Plant Watering** — soil moisture sensor + pump relay; web UI shows moisture graph, override, schedule
- **Garage Sentinel** — ultrasonic distance sensor detects car presence; pushes notification if garage left open
- **Power Monitor** — CT clamp on breaker panel + ADC; web dashboard shows real-time watts and daily kWh
- **Sous Vide Controller** — PID loop on thermocouple + SSR; web UI for target temp and timer

### Games
- **ESP32 Pong** — two-player Pong on ILI9341 TFT; each player has an encoder or potentiometer
- **Puzzle Box** — combination of inputs (encoder, button, keypad) must be solved in sequence to "unlock"; escapism prop
- **Simon Says** — classic game on 4 LEDs + buttons, with BLE high-score leaderboard
- **Reaction Timer** — LED + button; measures reaction time in ms, web leaderboard with names

### Mechatronics / Robots
- **Turntable Indexer** — stepper + encoder feedback; web UI sets angle steps for photography or machining
- **Camera Slider** — stepper-driven camera dolly, programmable move speed and distance, BLE trigger
- **Balancing Robot** — MPU6050 IMU + two DC motors + PID; classic self-balancing bot on dual-core ESP32
- **Drawing Robot** — two-servo or CoreXY pen plotter; accepts SVG paths via web upload
- **Robotic Arm** — 3-4 servo joints; web UI sliders per joint, record/playback motion sequences

### Learning / Educational
- **Oscilloscope Toy** — ESP32 ADC samples → WebSocket → browser canvas renders waveform in real time
- **Logic Analyzer** — 4-8 channels, captures digital edges, sends to browser for display
- **Signal Generator** — DAC + DMA → sine/square/triangle, frequency set via web UI
- **Retro Terminal** — serial-over-WebSocket; phone becomes a dumb terminal to another machine
- **Learn Morse** — sends random words as audio tone, you key back; tracks score and speed

### Fun / Weird
- **Geiger Counter Display** — SBM-20 tube + pulse counter; web UI shows CPM, dose rate, Geiger click sound
- **Ambient Noise Machine** — DAC → I2S amp; web UI selects noise color or rain/fire; BLE sleep timer
- **Name Badge** — BLE advertising your callsign; animated TFT display; detects nearby badges
- **Chaos Dice** — true RNG using ESP32 RF noise; rolls dice, keeps stats, detects bias
- **Magic 8-Ball Appliance** — shake sensor + speaker; web UI shows recent answers
