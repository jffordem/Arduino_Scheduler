# ESP32 Clicker — Claude Notes

## Project Layout

PlatformIO project inside the `jffordem_Scheduler` Arduino library repo.
Source is in `src/`; web UI assets are in `data/` (served from LittleFS).

## Build Environments

Two environments in `platformio.ini`:

- **`dev`** (default) — `ARDUINO_USB_MODE=1`, no `USB_HID_MODE`. USB CDC active
  on native port. Upload with PlatformIO Upload button or `flash.bat`. Serial on
  COM5 (CH343 UART bridge).

- **`hid`** — `ARDUINO_USB_MODE=0`, `ARDUINO_USB_CDC_ON_BOOT=0`, `USB_HID_MODE`
  defined. TinyUSB HID active; CDC disabled. Upload with `flash-hid.bat`. Serial
  still on COM5. Native USB-C port presents as HID mouse+keyboard to the game PC.

## Flashing

Always use `-t upload` — bare `pio run` only compiles, it does not flash.

```
flash.bat          → pio run -e dev -t upload
flash-hid.bat      → pio run -e hid -t upload
```

For the hid build, `upload_port = COM5` is set so esptool uses the UART bridge.
If the auto-download circuit doesn't trigger, do the manual dance first:
hold BOOT, tap RESET, release BOOT, then run `flash-hid.bat`.

The `all` argument to `flash-hid.bat` pauses between code and filesystem steps
so you can do the dance twice if auto-download doesn't work.

## Serial Monitoring

Serial output is always on COM5 (CH343 UART bridge / UART0), both in dev and
hid builds. Monitor with `pio device monitor` or PlatformIO's Monitor button.
`monitor_port = COM5` is set in both environments.

COM6 is the native USB port — it shows as a CDC serial device in dev mode and
as a HID device in hid mode. Never monitor COM6.

## Confirming Which Build Is Running

Press RST with serial monitor open on COM5. Look for the startup line:

- `USB HID initialised.` → hid build running ✓
- `HID stub mode (dev build) — USB Serial active.` → dev build running

## HID Debugging

In dev mode, every HID call logs to Serial instead of sending USB events:

```
[CMD] START (activeMode=AutoCast, running=no)
[HID] MOUSE PRESS   LEFT
[HID] MOUSE RELEASE LEFT
```

In hid mode, the same lines appear AND real USB HID events are sent. Use the
serial log to confirm the command pipeline is working before chasing USB issues.

## Adding a New Clicker Mode

1. Create `src/MyMode.h` extending `ClickerMode`
2. Implement `name()`, `start()`, `stop()`, `poll(uint32_t)`, `running()`,
   `statusJson()`, and `setParam(key, val)`
3. Add an instance and pointer in `main.cpp` (`modes[]` array)
4. Add a UI section in `data/index.html`
