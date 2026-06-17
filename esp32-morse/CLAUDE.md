# ESP32 Morse Trainer — Claude Notes

## Project Layout

PlatformIO project inside the `jffordem_Scheduler` Arduino library repo.
Source is in `src/`; web UI assets are in `data/` (served from LittleFS).
See `PROJECT-PLAN.md` for the full phase-by-phase spec.

## Build Environments

Two environments in `platformio.ini`:

- **`dev`** (default) — `ARDUINO_USB_MODE=1`, no `MORSE_ACTIVE`. USB CDC active.
  NeoPixel not driven; `LedOutput::mark()/space()` log to Serial instead.
  Upload with PlatformIO Upload button or `pio run -e dev -t upload`.

- **`morse`** — `MORSE_ACTIVE` defined. NeoPixel on GPIO48 is live.
  Normal deploy target. Use `flash.bat` or `flash.bat all`.

## Flashing

Always use `-t upload` — bare `pio run` only compiles, it does not flash.

```
flash.bat          → pio run -e morse -t upload         (code only)
flash.bat fs       → pio run -e morse -t uploadfs       (filesystem only)
flash.bat all      → code then filesystem
```

For the filesystem upload, `data/` must be populated before running `uploadfs`.
If `index.html` is missing, the web server will 404 on `/`.

## Serial Monitoring

Serial output is on COM5 (CH343 UART bridge / UART0) in both envs.
Monitor with `pio device monitor` or PlatformIO's Monitor button.

Press RST with monitor open. Startup confirms:
- `LED self-test complete.` — NeoPixel blinked 3× (morse build) or stubs ran (dev)
- `Connected. Open http://<ip>/` — WiFi up
- `Web server started.` — ready

## WiFi Credentials

Copy `src/secrets.h.template` to `src/secrets.h` and fill in your SSID/password.
`secrets.h` is gitignored.

## Thread Safety

`cmdQueue` is the only thread-safety boundary. The WebSocket handler (AsyncWebServer,
runs on Core 0) enqueues `MorseCmd` objects. `loop()` (Core 1) drains the queue
before polling — all state mutations happen on Core 1 only.
