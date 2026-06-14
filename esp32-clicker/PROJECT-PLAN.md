# ESP32 Clicker — Project Plan

See `README.md` for architecture, mode designs, and detailed design notes.
This document tracks what to build, in what order, and how to know each
phase is done.

---

## File Map

Every file created during the project, organized by the phase that
introduces it.

```
esp32-clicker/
├── README.md               ✓ done
├── PROJECT-PLAN.md         ✓ done (this file)
│
│── Phase 1: Skeleton ──────────────────────────────────────────────────────
├── platformio.ini          ✓ two envs: dev (UART0 Serial) and hid (TinyUSB)
├── flash.bat               ✓ convenience wrapper — flash / flash fs / flash all
├── .gitignore              ✓
├── src/main.cpp            ✓ setup() / loop() / WiFi / queue drain
├── src/secrets.h           ✓ WIFI_SSID / WIFI_PASSWORD  [gitignored]
├── src/Timer.h             ✓ forked from Clock.hpp — standalone countdown timer
├── src/ClickerCmd.h        ✓ ClickerCmd struct + QueueHandle_t declaration
├── src/HID.h               ✓ declarations
├── src/HID.cpp             ✓ TinyUSB wrappers + Serial stubs for dev mode
├── src/ClickerMode.h       ✓ abstract base class
├── src/WebUI.h             ✓ declarations
├── src/WebUI.cpp           ✓ AsyncWebServer + WebSocket; wsHandler → cmdQueue
└── data/
    └── index.html          ✓ placeholder replaced by Phase 2 UI

│── Phase 2: AutoCast ──────────────────────────────────────────────────────
├── src/AutoCast.h          ✓ implemented (HID testing pending)
└── data/index.html         ✓ input selector + hold/wait sliders + Start/Stop

│── Phase 3: HoldButton ────────────────────────────────────────────────────
├── modes/HoldButton.h      HoldButtonParams + state machine
└── data/index.html         +HoldButton UI section

│── Phase 4: KeySequence ───────────────────────────────────────────────────
├── KeyParser.h             text token → HID keycode
├── modes/KeySequence.h     KeySequenceParams + state machine
└── data/index.html         +KeySequence UI section

│── Phase 5: Macro ─────────────────────────────────────────────────────────
├── modes/Macro.h           MacroStep / MacroDef + EnchantingGrind sequence
└── data/index.html         +Macro UI section (dropdown only, no editing)

│── Phase 6: Polish ────────────────────────────────────────────────────────
    NVS persistence         add Preferences load/save to each mode
    WiFi fallback AP        captive-portal path in esp32-clicker.ino
    Error messages          WebSocket "error" messages + UI banner
    Reconnect banner        "clicker still running" notice in index.html
```

---

## Phase 1 — Skeleton

**Goal:** The sketch compiles and boots. WiFi connects. The web server serves
a placeholder page. `loop()` drains the command queue and runs a no-op mode.
Nothing crashes. No HID, no real modes yet.

### Files

**`platformio.ini` — two environments**

`USB_HID_MODE` is set via build flags in `platformio.ini`, not via a header
file. No file edits needed to switch modes:
- `flash` / `flash fs` / `flash all` — dev env; Serial on UART0, no HID
- `pio run -e hid` / `pio run -e hid -t uploadfs` — TinyUSB HID

Note: the dev env uses `ARDUINO_USB_MODE=1` (hardware USB Serial/JTAG) but
does NOT set `ARDUINO_USB_CDC_ON_BOOT`, so `Serial` routes to UART0. The
ROM bootloader messages and sketch Serial output both appear on the same
COM port (the USB-to-UART bridge chip), making monitoring straightforward.

`board_build.filesystem = littlefs` in the shared `[env]` block tells
PlatformIO to package `data/` as a LittleFS image for both environments.
LittleFS is part of the arduino-esp32 core; it needs no `lib_deps` entry.

**`Timer.h`** — forked from `Clock.hpp`'s `Timer` class.

Changes from the original:
- Remove the `Expires` base class (not needed here)
- Change `long` to `uint32_t` throughout (matches `millis()` return type)
- No `#include <Scheduler.hpp>` dependency
- Keep `expired()`, `reset()`, `reset(uint32_t time)`

```cpp
class Timer {
    uint32_t _time;
    uint32_t _lastReset;
public:
    Timer(uint32_t time = 0) : _time(time), _lastReset(millis()) {}
    bool expired() const { return millis() - _lastReset > _time; }
    void reset()                { _lastReset = millis(); }
    void reset(uint32_t time)   { _time = time; _lastReset = millis(); }
    void disarm()               { _time = UINT32_MAX; _lastReset = millis(); }
};
```

**`ClickerCmd.h`**

```cpp
struct ClickerCmd {
    enum Type { SET_MODE, START, STOP, SET_PARAM } type;
    char paramKey[20];
    char paramVal[64];
};

extern QueueHandle_t cmdQueue;
```

**`HID.h`**

```cpp
namespace HID {
    void begin();
    void pressKey(uint8_t code);
    void releaseKey(uint8_t code);
    void tapKey(uint8_t code);       // press + short delay + release
    void pressMouse(uint8_t btn);    // MOUSE_BUTTON_LEFT / RIGHT
    void releaseMouse(uint8_t btn);
    void releaseAll();
    uint8_t charToCode(char c);      // 'a'-'z', '0'-'9' → HID keycode
}
```

When `USB_HID_MODE` is not defined, every function logs to Serial and returns.
When defined, calls the TinyUSB keyboard/mouse API.

Mouse button constants to define (TinyUSB uses different names than Arduino):
```cpp
#define MOUSE_BUTTON_LEFT   0x01
#define MOUSE_BUTTON_RIGHT  0x02
```

**`ClickerMode.h`**

```cpp
class ClickerMode {
public:
    virtual void        start()                     = 0;
    virtual void        stop()                      = 0;
    virtual void        poll(uint32_t now)          = 0;
    virtual bool        running()           const   = 0;
    virtual const char* name()              const   = 0;
    virtual void        setParam(const char* key,
                                 const char* val)   = 0;
    virtual String      statusJson()        const   = 0;
    virtual void        loadFromNVS()               {}  // optional; Phase 6
    virtual void        saveToNVS()                 {}  // optional; Phase 6
};
```

**`WebUI.h`**

Sets up `AsyncWebServer` and `AsyncWebSocket`. The WebSocket `onEvent`
handler does one thing only: parses the incoming JSON, fills a `ClickerCmd`,
and calls `xQueueSend(cmdQueue, &cmd, 0)`. It never touches mode state.

Routes:
- `GET /` → `LittleFS /index.html`
- `GET /macros` → JSON array of built-in macro names
- `WS /ws` → WebSocket

Exposes:
```cpp
void webui_begin(ClickerMode** modes, int modeCount);
void webui_push(const String& json);    // called from loop() to broadcast
void webui_cleanup();                   // ws.cleanupClients()
```

**`esp32-clicker.ino`**

```cpp
#include "Config.h"
#include "ClickerCmd.h"
#include "HID.h"
#include "ClickerMode.h"
#include "WebUI.h"

// (modes included in later phases)

QueueHandle_t cmdQueue;
ClickerMode*  activeMode   = nullptr;
uint32_t      lastWsPush   = 0;

void applyCommand(const ClickerCmd& cmd) {
    switch (cmd.type) {
        case ClickerCmd::START:     if (activeMode) activeMode->start(); break;
        case ClickerCmd::STOP:      if (activeMode) activeMode->stop();  break;
        case ClickerCmd::SET_PARAM: if (activeMode) activeMode->setParam(cmd.paramKey, cmd.paramVal); break;
        case ClickerCmd::SET_MODE:  /* Phase 2+ */ break;
    }
}

void setup() {
    Serial.begin(115200);
    cmdQueue = xQueueCreate(16, sizeof(ClickerCmd));

    // Phase 1: activeMode stays nullptr; loop() guards against it

    #ifdef USB_HID_MODE
        USB.begin();
        HID::begin();
    #endif

    // WiFi — Phase 1 uses secrets.h only; captive portal added in Phase 6
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) delay(250);
    Serial.println(WiFi.localIP());

    LittleFS.begin(true);
    webui_begin(nullptr, 0);
}

void loop() {
    ClickerCmd cmd;
    while (xQueueReceive(cmdQueue, &cmd, 0) == pdTRUE)
        applyCommand(cmd);

    uint32_t now = millis();
    if (activeMode) activeMode->poll(now);

    if (now - lastWsPush >= 200) {
        if (activeMode) webui_push(activeMode->statusJson());
        lastWsPush = now;
    }
    webui_cleanup();
}
```

### Done When

- Compiles without error (with and without `USB_HID_MODE`)
- ESP32 connects to WiFi and prints its IP
- Browser at `http://<ip>/` receives the placeholder HTML
- WebSocket connects (verify with browser devtools)
- No crashes after 5 minutes idle

---

## Phase 2 — AutoCast

**Goal:** AutoCast is selectable, configurable, and functional. Start/Stop
work. In non-HID mode, Serial shows PRESS/RELEASE on schedule. In HID mode,
the selected key or mouse button fires at the configured timing.

**Status:** Implemented; HID mode not yet tested against a live game.

### Design (as built)

A single configurable input (key or mouse button) fires on a non-blocking
two-state machine:

```
start() → HOLDING (press input)
  → after holdMs → WAITING (release input)
  → after waitMs → HOLDING (press input again)
  → ...
stop()  → release if HOLDING → IDLE
```

Using `millis()` deltas throughout — no `delay()` calls in `poll()`.

**`src/AutoCast.h`** — implements `ClickerMode`.

`setParam()` keys:
- `input` — `"left"`, `"right"`, `"middle"`, or a single ASCII char (e.g. `"r"`)
- `holdMs` — how long to hold the input down (default 1200 ms)
- `waitMs` — gap between casts (default 800 ms)

`statusJson()` example:
```json
{"mode":"AutoCast","running":false,"input":"left","holdMs":1200,"waitMs":800}
```

### `data/index.html` (as built)

- Input selector: Left Mouse / Right Mouse / Middle Mouse / Key… (reveals char input)
- Hold slider: 50 ms – 3000 ms
- Wait slider: 100 ms – 5000 ms
- Large START / STOP button; status badge updates via WebSocket heartbeat

### `flash.bat`

Since `platformio.exe` is not on PATH, a `flash.bat` convenience script
wraps the full path to the PlatformIO executable:
- `flash`      — flash code only (`pio run -e dev`)
- `flash fs`   — flash filesystem only (`pio run -e dev -t uploadfs`)
- `flash all`  — flash code then filesystem

### Done When

- [ ] Compiles with and without `USB_HID_MODE`
- [ ] Start/Stop toggle works in browser; Serial shows PRESS/RELEASE in dev build
- [ ] Muffle spell cast (holdMs 1200, waitMs 800, Left Mouse) works in Skyrim
- [ ] Params change mid-run without crashing

---

## Phase 3 — HoldButton

**Goal:** HoldButton mode holds mouse buttons continuously. Toggle off releases
immediately.

### Files

**`modes/HoldButton.h`**

`poll()` is a no-op. All behavior is in `start()`, `stop()`, and `setParam()`.

`setParam()` keys: `leftHeld`, `rightHeld` (bool).

`statusJson()`:
```json
{
  "type": "status", "mode": "HoldButton", "running": false,
  "params": { "leftHeld": false, "rightHeld": false }
}
```

### `data/index.html`

Add HoldButton param section: two large toggle switches. No interval fields.

### Done When

- Selecting HoldButton shows its param section
- Start arms the mode; toggling leftHeld/rightHeld presses/releases immediately
- Stop releases both buttons regardless of toggle state
- Re-selecting AutoCast releases any held buttons first

---

## Phase 4 — KeySequence

**Goal:** A user-editable key list cycles in order. The Fallout gambling
sequence `3 1` works out of the box.

### Files

**`KeyParser.h`**

`uint8_t parseToken(const char* token)` — maps a text token to a TinyUSB
HID keycode.

Supported tokens:
- Single printable char: `'a'`–`'z'`, `'0'`–`'9'`, `' '` → `HID_KEY_SPACE`
- `{ENTER}`, `{ESC}`, `{SPACE}`, `{TAB}`
- `{LEFT}`, `{RIGHT}`, `{UP}`, `{DOWN}`
- `{F1}`–`{F12}`
- `{LMOUSE}`, `{RMOUSE}` (handled by KeySequence as mouse, not keyboard)

Returns `0` for unknown tokens (logged as warning, skipped).

**`modes/KeySequence.h`**

`setParam()` keys:
- `keys` — space-separated token string, e.g. `"3 1"` or `"{LEFT} {ENTER}"`
- `pressDuration`, `keyDelay` (uint32_t ms)

Parsing `keys` re-runs `parseToken()` for each token and stores the result
array. Parsing happens in `setParam()`, not in `poll()`.

`statusJson()`:
```json
{
  "type": "status", "mode": "KeySequence", "running": false,
  "params": { "keys": "3 1", "pressDuration": 25, "keyDelay": 250 }
}
```

### `data/index.html`

Add KeySequence param section:
- Text input for key sequence (single line, e.g. `3 1`)
- Number fields for press duration and key delay
- Small legend linking to the key syntax table in README

### Done When

- Default sequence `3 1` runs on Start
- Editing the key field and hitting Enter (or after 300 ms debounce) takes
  effect on the current or next cycle
- Invalid tokens are skipped silently; no crash
- `{F5}` token works (needed to verify special key path)

---

## Phase 5 — Macro

**Goal:** The EnchantingGrind macro runs. Macro selection from a dropdown
populated from the firmware's built-in macro table.

### Files

**`modes/Macro.h`**

```cpp
enum MacroStepType { PRESS_KEY, PRESS_MOUSE, PAUSE };

struct MacroStep {
    MacroStepType type;
    uint8_t       code;       // HID keycode or mouse button; 0 for PAUSE
    uint16_t      holdMs;     // how long to hold (PRESS steps); 0 for PAUSE
    uint16_t      afterMs;    // delay after release (or total for PAUSE)
};

struct MacroDef {
    const char*      name;
    const MacroStep* steps;
    int              stepCount;
};
```

`EnchantingGrind` defined as a `const MacroStep[]` array. Steps:
- Arrow keys to navigate to enchanting table position
- ENTER to select item, enchantment, soul gem
- Character keys to type the filter string (`fMagicry` or similar)
- Repeat

`setParam()` key: `macroIndex` (int, 0-based into built-in macro table).

`GET /macros` route returns JSON array of macro names for the UI dropdown.

`statusJson()`:
```json
{
  "type": "status", "mode": "Macro", "running": false,
  "params": { "macroIndex": 0, "macroName": "EnchantingGrind" }
}
```

### `data/index.html`

Add Macro param section: dropdown of macro names (populated from `GET /macros`
on page load). No other editable fields.

### Done When

- Macro dropdown shows "EnchantingGrind"
- Start runs the sequence; stop halts and releases any held key
- Sequence loops correctly (step index wraps)
- Timing matches the original sketch (25 ms hold, 250 ms after)

---

## Phase 6 — Polish

**Goal:** The device is self-contained. WiFi credentials don't need to be
hardcoded. Settings survive power cycles. Errors are surfaced in the UI.

### Tasks (no new files — changes spread across existing files)

**NVS persistence** — Add `loadFromNVS()` and `saveToNVS()` to each mode.
Call `loadFromNVS()` in `setup()` after the modes are constructed. Call
`saveToNVS()` inside `setParam()` after applying each change.

**WiFi captive portal** — After `WiFi.begin()` times out (10 s), fall back
to `WiFi.softAP("ESP32-Clicker", "clicker1")`. Add a `/setup` route that
serves a small WiFi credential form. Saving credentials writes to NVS
(`wifi.ssid`, `wifi.pass`) and reboots.

**Error messages** — `applyCommand()` detects invalid states (e.g. Start
with empty KeySequence key list) and calls `webui_push()` with an error JSON:
```json
{ "type": "error", "message": "KeySequence: no keys defined" }
```
UI shows a dismissible red banner below the header.

**Reconnect banner** — When the WebSocket reconnects and the first status
message shows `"running": true`, show a yellow banner:
`"Clicker is still running — tap STOP to halt."` Dismissed by tapping or
on STOP.

### Done When

- Power cycle restores all params to their last saved values
- Device boots to the captive portal if no WiFi credentials are saved
- Entering credentials via the portal connects to the home network and
  persists across reboots
- Trying to Start with an empty key list shows an error banner, not a crash
- Reconnecting after phone sleep shows the "still running" banner if applicable

---

## Testing Strategy

### Serial logging (non-HID mode)

When `USB_HID_MODE` is not defined, `HID::pressKey()` / `releaseMouse()` / etc.
print to Serial:

```
[1234] KEY PRESS   HID_KEY_F5
[1334] KEY RELEASE HID_KEY_F5
[2500] MOUSE PRESS LEFT
[2600] MOUSE RELEASE LEFT
```

This lets every mode be fully tested through the web UI before the device
ever needs to be in HID mode. Most development happens in non-HID mode.

### HID mode testing

Switching to HID mode requires:
1. Uncomment `#define USB_HID_MODE` in `Config.h`
2. Re-flash: hold BOOT, tap RESET, release BOOT, then upload
3. The device enumerates as a USB HID device; Serial is on hardware UART pins
   (not USB) — use a USB-to-serial adapter on IO43/IO44 if Serial output is
   needed while in HID mode

Test each mode in a text editor first (keyboard output is visible) before
switching to the game.

### Phase gate checklist

Before moving from one phase to the next:
- [ ] Compiles with and without `USB_HID_MODE`
- [ ] No crash after 10 minutes idle
- [ ] No crash after 5 minutes running the active mode
- [ ] WebSocket reconnects cleanly after killing and reopening the browser tab
- [ ] Mode switching (any → any) releases all held keys/buttons

---

## Known Risks

**TinyUSB HID API surface** — The exact function calls for keyboard/mouse in
arduino-esp32 ≥ 2.0 may differ from what's documented. Validate `HID.h`
early in Phase 1 (even if just calling `begin()` and checking it doesn't
crash) before building modes on top of it.

**LittleFS upload** — `index.html` must be uploaded to the ESP32's flash
separately from the sketch, using the Arduino IDE LittleFS data uploader
plugin or `esptool`. Factor this into the flash-and-test cycle.

**`millis()` rollover** — `Timer` uses `millis() - _lastReset > _time`
which handles the 49-day `uint32_t` rollover correctly (subtraction wraps).
The mode state machines use `now >= nextTime` comparisons — these do NOT
handle rollover. Rewrite those comparisons as `now - nextTime < someLimit`
if rollover safety is needed. At typical play session lengths it won't matter.

**AsyncWebServer and LittleFS concurrency** — Serving `index.html` from
LittleFS inside an async handler while `loop()` is running is safe because
file reads are on Core 0 (the async task) and mode state is on Core 1.
No shared mutable state between them.
