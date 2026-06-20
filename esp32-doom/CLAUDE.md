# ESP32 Doom — Claude Notes

## What This Is

Browser-hosted Doom served from an ESP32-S3. The ESP32 runs a plain HTTP file
server; a phone or PC browser connects and runs Doom entirely client-side via
WebAssembly. No display, no audio chip, no extra hardware — just the N16R8
board and WiFi.

This is the same "ESP32 as appliance, browser as UI" pattern as ESP32-Zork,
but even simpler: no WebSocket, no interpreter — just static file serving.

## Architecture

```
Browser (phone/PC)          ESP32-S3
──────────────────          ────────────────────────────────
loads index.html      ←     AsyncWebServer (HTTP)
loads doom.js/.wasm   ←     LittleFS (serves static bundle)
fetches doom1.wad     ←     LittleFS (WAD file, ~4 MB)
runs Doom in canvas         (nothing — just files)
```

The ESP32 does no game logic. Once the browser has downloaded the files, the
ESP32 is idle. Works over WiFi AP or station mode.

## WASM Doom Bundle

Use **wasm-doom** (github.com/lazarv/wasm-doom) — a Chocolate Doom build
compiled via Emscripten. It produces:

- `index.html` — canvas + JS loader
- `doom.js` — Emscripten glue
- `doom.wasm` — compiled game engine
- WAD loaded at runtime via a URL (not baked in)

**WAD file:** `doom1.wad` (shareware, ~4.1 MB). Fits in LittleFS alongside
the ~2 MB WASM bundle. Total flash usage ~6–7 MB — well within the 16 MB
N16R8 with a custom partition table.

The bundle's WAD loading mechanism needs verification: Emscripten builds
typically use a virtual filesystem populated via `--preload-file` (WAD baked
into the JS bundle) or a `fetch()` at runtime. We want the fetch approach so
the WAD lives separately in LittleFS and the JS bundle stays small.

If wasm-doom doesn't support runtime WAD fetching cleanly, alternative:
**js-doom** (github.com/mozilla/BananaBread or similar pure-JS ports) which
load the WAD via `fetch()` naturally.

## Project Layout

PlatformIO project, same structure as esp32-zork:

```
esp32-doom/
├── CLAUDE.md
├── platformio.ini
├── partitions.csv          (custom: large LittleFS)
├── flash.bat               (same helper as zork)
├── src/
│   └── main.cpp            (WiFi + AsyncWebServer, ~80 lines)
└── data/                   (uploaded to LittleFS)
    ├── index.html          (modified from wasm-doom to fetch WAD from /doom1.wad)
    ├── doom.js
    ├── doom.wasm
    └── doom1.wad
```

## Flashing

```
flash.bat          → compile + flash firmware
flash.bat fs       → upload LittleFS (data/ directory)
flash.bat all      → firmware then filesystem
```

`flash.bat fs` must be run after any change to `data/`. The WAD is large;
the first `flash fs` will take a minute or two.

## Access

- IP: printed on boot via serial
- mDNS: http://doom.local/

## Serial / Debug

UART0 on COM5 at 115200 baud. On boot prints IP + mDNS name.

## Partition Table

Default partition gives ~1.5 MB to LittleFS — not enough. Use custom:

```ini
; platformio.ini
board_build.partitions = partitions.csv
```

`partitions.csv` should give ~12 MB to LittleFS (firmware ~1.5 MB is plenty
for a static file server).

## Milestone Status

1. **Bringup:** WiFi + AsyncWebServer + "hello" page from LittleFS — confirm
   HTTP serving works over phone browser
2. **WASM bundle:** Add doom.js + doom.wasm to data/; confirm bundle loads
   and initializes in browser (no WAD yet — expect an error, but engine loads)
3. **WAD serving:** Add doom1.wad to data/; wire index.html to fetch it from
   `/doom1.wad`; Doom runs in browser
4. **Full game:** End-to-end playable Doom from phone browser on ESP32 WiFi
5. **Polish:** mDNS as doom.local, keyboard/gamepad controls hint overlay,
   cache headers so browser doesn't re-fetch WAD on reload

## main.cpp Sketch

The firmware is intentionally minimal — no game logic, just file serving:

```cpp
// WiFi connect → LittleFS mount → AsyncWebServer on port 80
// serve everything in /littlefs/ as static files
// MIME types: .wasm → application/wasm, .wad → application/octet-stream
// mDNS: doom.local
```

One subtlety: `.wasm` files must be served with `Content-Type: application/wasm`
or some browsers refuse to compile them. `AsyncWebServer` needs a custom MIME
entry for this.

## Key Decisions / Open Questions

- **WAD runtime fetch vs preload:** Verify wasm-doom supports `fetch()` WAD
  loading, or patch index.html to pass WAD as an Emscripten FS file mounted
  from a fetch response.
- **Bundle size:** If doom.js + doom.wasm + doom1.wad exceeds LittleFS budget,
  serve doom.wasm from a URL the user provides (phone/PC local file) — but
  try fitting it first, 16 MB is generous.
- **Audio:** Chocolate Doom has audio support; Emscripten can route it to the
  Web Audio API. May just work. If it causes bundle size issues, disable
  (`--no-sound` or equivalent compile flag).
- **Controls:** Keyboard works on desktop; touch controls on phone need an
  overlay or gamepad API. Out of scope for v1.
