# ESP32 Clicker

A WiFi-controlled USB HID clicker for single-player game automation. The ESP32-S3
presents as a USB keyboard and mouse to the gaming PC while simultaneously serving
a control web page reachable from any phone or tablet on the same network.

Replaces four Leonardo-based sketches with a single device that covers all their
use cases and adds a browser UI so you never need to touch the device mid-game.

| Old Sketch | What it did | Clicker mode |
|---|---|---|
| DualCastMuffle | Left/right mouse clicks at configurable intervals | AutoCast |
| MouseButtonToggle | Hold mouse button until toggled off | HoldButton |
| Fallout_1_Gamble | Rapid alternating keypresses in a loop | KeySequence |
| EnchantingGrind | 30-step navigation sequence, looping | Macro |

---

## Hardware

- ESP32-S3 (N16R8 or similar with native USB OTG)
- USB-C cable: ESP32-S3 → gaming PC (carries both power and HID)
- Phone or tablet on the same WiFi network as the ESP32

No additional hardware. The ESP32-S3's USB port acts as both power input and HID
device — the game PC sees it as a standard USB keyboard + mouse.

---

## How It Works

```
Phone / Tablet                        Gaming PC
      │                                    │
      │  WiFi — browser UI                 │  USB HID (keyboard + mouse)
      ▼                                    ▼
┌──────────────────────────────────────────────────────┐
│                      ESP32-S3                        │
│                                                      │
│  AsyncWebServer ◄──► ClickerState ◄──► Mode engine  │
│  WebSocket                                │          │
│                                      TinyUSB HID     │
└──────────────────────────────────────────────────────┘
```

The ESP32 runs two things in parallel:

- A **web server** (port 80) that serves the control UI and accepts commands via
  WebSocket. The WebSocket also pushes status updates back to the browser so the
  display stays live without polling.

- A **mode engine** polled in `loop()` that drives the active clicker mode. Each
  mode is a small state machine that calls into the HID layer on its own schedule.

---

## Clicker Modes

### AutoCast
Presses and releases the left mouse button, right mouse button, or both on
independent timers. Designed for Skyrim spell-casting to level magic skills.

Parameters (all adjustable from the web UI):
- Left hand: enabled/disabled, interval in ms (default 1600 ms)
- Right hand: enabled/disabled, interval in ms (default 600 ms)
- Press duration in ms (default 100 ms — how long the button is held down)
- Auto-save: periodically presses F5 (default every 60 seconds)

### HoldButton
Holds the left mouse button, right mouse button, or both down continuously.
Toggling off releases. Designed for Subnautica Prawn Suit mining where the game
expects a sustained hold rather than repeated clicks.

Parameters:
- Left mouse button: hold on / off
- Right mouse button: hold on / off

### KeySequence
Cycles through a short ordered list of keypresses indefinitely. Designed for
Fallout casino gambling, where the sequence is `3 → 1 → 3 → 1 → …`

Parameters (editable from the web UI):
- Key list: ordered list of keys (text input, see key syntax below)
- Press duration in ms (default 25 ms)
- Delay between keys in ms (default 250 ms)

### Macro
Runs a named multi-step sequence defined in firmware. Each step is one of:
press key, release key, or delay N ms. The sequence loops until stopped.
Macros are defined at compile time (editing the firmware) because they tend
to be long and game-specific.

Built-in macros:
- **EnchantingGrind** — navigates the Skyrim enchanting table UI: reset position,
  select item, select enchantment, select soul gem, type filter string, repeat.

Parameters: select macro by name, then Start / Stop only.

---

## Web UI

Single-page app stored in ESP32 flash (LittleFS). Designed for phone portrait
mode with large touch targets.

```
┌──────────────────────┐
│  ESP32 Clicker  [●]  │  ← status dot: green = running, grey = idle
├──────────────────────┤
│  Mode: [AutoCast  ▼] │  ← dropdown; switching mode stops any active run
├──────────────────────┤
│                      │
│  ── AutoCast ──────  │
│                      │
│  Left hand    [ON ●] │  ← toggle
│  Interval   [1600] ms│
│                      │
│  Right hand  [OFF ●] │
│  Interval    [600] ms│
│                      │
│  Press time   [100] ms
│                      │
│  Auto-save F5 [ON ●] │
│  Every         [60] s│
│                      │
├──────────────────────┤
│      [ START ]       │  ← green; disabled while running
│      [ STOP  ]       │  ← red; disabled while idle
└──────────────────────┘
```

Each mode shows its own parameter section. Parameter changes take effect
immediately if the mode is already running (e.g. adjusting interval mid-cast).
Settings are saved to NVS and restored on next power-up.

---

## Key Syntax (KeySequence)

Keys in the sequence list are entered as text with `{name}` escapes for
special keys:

| Input | Meaning |
|---|---|
| `a` – `z`, `0` – `9` | Literal character |
| `{ENTER}` | Enter / Return |
| `{SPACE}` | Space bar |
| `{LEFT}` `{RIGHT}` `{UP}` `{DOWN}` | Arrow keys |
| `{F1}` – `{F12}` | Function keys |
| `{ESC}` | Escape |
| `{LMOUSE}` `{RMOUSE}` | Mouse buttons (for mixed key+mouse sequences) |

Example sequence for Fallout gambling: `3 1`
(space-separated; each token is one keypress)

---

## Code Structure

```
esp32-clicker/
├── esp32-clicker.ino     # setup(), loop(), USB + WiFi init
├── secrets.h             # WIFI_SSID / WIFI_PASSWORD — gitignored
├── Config.h              # compile-time flags (USB_HID_MODE, etc.)
├── ClickerState.h        # shared state: active mode, params, running flag
├── WebUI.h / .cpp        # AsyncWebServer + WebSocket, serves /data/index.html
├── HID.h / .cpp          # TinyUSB wrappers: pressKey, releaseKey, clickMouse, holdMouse
├── modes/
│   ├── ClickerMode.h     # abstract base: start(), stop(), poll(now), running()
│   ├── AutoCast.h        # AutoCast state machine
│   ├── HoldButton.h      # HoldButton state machine
│   ├── KeySequence.h     # KeySequence state machine
│   └── Macro.h           # Macro runner + built-in macro definitions
└── data/
    └── index.html        # Web UI (upload to LittleFS with Arduino IDE data uploader)
```

### ClickerMode interface

```cpp
class ClickerMode {
public:
    virtual void        start()              = 0;
    virtual void        stop()               = 0;
    virtual void        poll(uint32_t now)   = 0;  // called every loop()
    virtual bool        running()     const  = 0;
    virtual const char* name()        const  = 0;
    virtual String      statusJson()  const  = 0;  // sent over WebSocket
};
```

Each mode owns its own parameter struct. `ClickerState` holds a pointer to the
active mode and delegates `poll()` calls from `loop()`.

### WebSocket protocol

Messages are JSON. Browser → ESP32:

```json
{ "cmd": "start" }
{ "cmd": "stop" }
{ "cmd": "setMode", "mode": "AutoCast" }
{ "cmd": "setParam", "key": "leftInterval", "value": 800 }
```

ESP32 → browser (pushed on state change and every ~200 ms while running):

```json
{ "mode": "AutoCast", "running": true, "leftEnabled": true, "leftInterval": 1600, ... }
```

---

## WiFi Setup

**Option A — secrets.h (recommended for home use)**

Define `WIFI_SSID` and `WIFI_PASSWORD` in `secrets.h` before flashing. The ESP32
connects at boot and prints its IP to the serial monitor.

**Option B — captive portal (first-boot fallback)**

If credentials are missing or the saved network is unreachable, the ESP32 starts
an access point named `ESP32-Clicker` (password: `clicker1`). Connect from the
phone and navigate to `192.168.4.1`. A setup page lets you enter the home network
credentials; they're saved to NVS and used on all future boots.

---

## Flashing & Development

### Hardware — two USB ports

The ESP32-S3-DevKitC-1 has two USB connectors that serve different purposes:

| Port | Chip | Windows device | Purpose |
|---|---|---|---|
| Micro-USB | CH343 UART bridge | COM5 (or similar) | Serial monitor + programming |
| USB-C | Native ESP32-S3 USB | COM6 in dev / HID device in hid | HID output to gaming PC |

**Serial output always comes through COM5** (UART0 via CH343), regardless of
which build is running. Keep the serial monitor pointed there.

### Two build environments (platformio.ini)

| Environment | What it does | How to flash |
|---|---|---|
| `dev` (default) | HID disabled, USB CDC on native port | `flash.bat` or PlatformIO Upload button |
| `hid` | TinyUSB HID active, CDC disabled | `flash-hid.bat` (see below) |

When `USB_HID_MODE` is not defined (dev build), the sketch runs the full web
server and state machine but replaces every HID call with a `Serial.printf` —
useful for iterating on logic and the UI without reflashing in HID mode.

### Flashing scripts

```
flash.bat          # dev build — code only
flash.bat fs       # dev build — filesystem only
flash.bat all      # dev build — code then filesystem

flash-hid.bat      # hid build — code only
flash-hid.bat fs   # hid build — filesystem only
flash-hid.bat all  # hid build — code then filesystem (prompts between steps)
```

Both scripts use `pio run -t upload` (not bare `pio run`, which only compiles).

### Flashing the hid build

`flash-hid.bat` uploads via COM5 using esptool. If the board's auto-download
circuit triggers automatically, no button press is needed. If esptool times out
with a connection error, do the manual dance first:

1. Hold **BOOT**, tap **RESET**, release **BOOT**
2. Immediately run `flash-hid.bat`

After a successful hid flash, COM6 changes from a CDC serial port to a HID
device. Windows may take a few seconds to recognize the new HID mouse/keyboard.
Press **RST** once if the device doesn't reboot automatically after flashing.

---

## Dependencies

All available through the Arduino Library Manager or bundled with arduino-esp32:

| Library | Source | Purpose |
|---|---|---|
| AsyncTCP | Library Manager | Async TCP for the web server |
| ESPAsyncWebServer | Library Manager | Web server + WebSocket |
| USB / TinyUSB (arduino-esp32 ≥ 2.0) | Built in | HID keyboard + mouse |
| Preferences (arduino-esp32) | Built in | NVS storage for settings |
| LittleFS (arduino-esp32) | Built in | Flash filesystem for index.html |
| FreeRTOS | Built in (part of ESP-IDF) | Command queue for thread safety |
| jffordem_Scheduler (`Timer` only) | This repo | Countdown timer used in mode state machines |

Board: **ESP32S3 Dev Module** in Arduino IDE. USB Mode: **USB-OTG (TinyUSB)**.

**Note on jffordem_Scheduler:** `MainSchedule` is not used — only `Timer`
from `Clock.hpp`, which is a self-contained 15-line class with no threading
assumptions. `KeyPress` / `MouseButton` / `ButtonController` are not used
because they call the Arduino Leonardo HID API (`Keyboard.press()` /
`Mouse.press()`), which is different from the TinyUSB API on ESP32-S3.

---

## Open Questions

**Safety stop on disconnect** — If the phone's browser closes or the WiFi drops,
should the clicker stop? Recommendation: keep running (the game is probably
unattended and this is intentional). On reconnect, the UI shows the current state
and a "clicker is still running" banner so nothing is surprising.

**Multiple simultaneous modes** — e.g. AutoCast and HoldButton at once. Not in
scope for v1; one active mode at a time keeps the UI simple. Can revisit if a
real use case comes up.

**Macro editing in the UI** — The EnchantingGrind macro has 30+ steps; editing
those in a phone browser would be painful. Keep macros as firmware-defined
constants for now. KeySequence covers the short-sequence case.

---

## Detailed Design

### Parameter Structs

Each mode owns a struct that holds its full configuration. These are the
authoritative field names — NVS keys, WebSocket param names, and UI element
IDs all derive from them.

```cpp
struct AutoCastParams {
    bool     leftEnabled   = false;
    uint32_t leftInterval  = 1600;   // ms between left-click cycles
    bool     rightEnabled  = false;
    uint32_t rightInterval = 600;    // ms between right-click cycles
    uint32_t pressDuration = 100;    // ms to hold button down per click
    bool     autoSave      = true;
    uint32_t saveInterval  = 60000;  // ms between F5 presses
};

struct HoldButtonParams {
    bool leftHeld  = false;
    bool rightHeld = false;
};

struct KeySequenceParams {
    static constexpr int MAX_KEYS = 16;
    char     keys[MAX_KEYS][16] = {};   // token per slot, e.g. "3", "{ENTER}"
    int      keyCount           = 2;    // default: "3 1"
    uint32_t pressDuration      = 25;   // ms to hold each key
    uint32_t keyDelay           = 250;  // ms between key releases and next press
};

struct MacroParams {
    int macroIndex = 0;   // index into the built-in macro table
};
```

---

### State Machine Behavior

Each mode's `poll(uint32_t now)` is called every `loop()` iteration.
`millis()` is passed in so all modes share a consistent timestamp.

#### AutoCast

Two independent timers (left and right) each cycle through WAITING → PRESSING
→ WAITING. A third timer handles auto-save.

```
on start(now):
    leftNextTime  = now          ← press immediately on next poll
    rightNextTime = now
    saveNextTime  = now + saveInterval
    leftPhase = rightPhase = WAITING

on poll(now):
    for each side (left, right):
        if enabled and phase == WAITING and now >= nextTime:
            pressMouse(side)
            phase     = PRESSING
            nextTime  = now + pressDuration
        if phase == PRESSING and now >= nextTime:
            releaseMouse(side)
            phase     = WAITING
            nextTime  = now + interval

    if autoSave and now >= saveNextTime:
        tapKey(KEY_F5)           ← press + release in one call
        saveNextTime = now + saveInterval

on stop():
    releaseMouse(LEFT); releaseMouse(RIGHT)
    leftPhase = rightPhase = IDLE

on setParam(leftEnabled, false) while running:
    if leftPhase == PRESSING: releaseMouse(LEFT)
    leftPhase = IDLE

on setParam(leftEnabled, true) while running:
    leftPhase    = WAITING
    leftNextTime = now           ← start on next poll
```

#### HoldButton

No timer — purely reactive to parameter changes. `poll()` is a no-op.
`running()` returns true when the mode is started, regardless of hold state
(so the UI's Start/Stop reflect arming, not hold state).

```
on start():
    _running = true
    if leftHeld:  pressMouse(LEFT)
    if rightHeld: pressMouse(RIGHT)

on stop():
    releaseMouse(LEFT); releaseMouse(RIGHT)
    _running = false

on setParam(leftHeld, value) while running:
    if value: pressMouse(LEFT)
    else:     releaseMouse(LEFT)
    leftHeld = value             ← param updated; saved to NVS

on poll(now): (nothing)
```

#### KeySequence

Single phase variable alternates PRESSING and DELAY across all keys in order.

```
on start(now):
    currentKey = 0
    pressKey(keys[0])
    phase    = PRESSING
    nextTime = now + pressDuration

on poll(now):
    if phase == PRESSING and now >= nextTime:
        releaseKey(keys[currentKey])
        phase    = DELAY
        nextTime = now + keyDelay

    if phase == DELAY and now >= nextTime:
        currentKey = (currentKey + 1) % keyCount
        pressKey(keys[currentKey])
        phase    = PRESSING
        nextTime = now + pressDuration

on stop():
    if phase == PRESSING: releaseKey(keys[currentKey])
    reset state
```

#### Macro

Steps are `{ type, code, holdMs, afterMs }`. `type` is one of `PRESS_KEY`,
`PRESS_MOUSE`, or `PAUSE` (no key, just wait `afterMs`).

```
on start(now):
    currentStep = 0
    executeStep(now)

executeStep(now):
    step = steps[currentStep]
    if step.type == PAUSE:
        phase    = PAUSING
        nextTime = now + step.afterMs
    else:
        press(step.type, step.code)
        phase    = PRESSING
        nextTime = now + step.holdMs

on poll(now):
    if phase == PRESSING and now >= nextTime:
        release(steps[currentStep])
        phase    = PAUSING
        nextTime = now + steps[currentStep].afterMs

    if phase == PAUSING and now >= nextTime:
        currentStep = (currentStep + 1) % stepCount
        executeStep(now)

on stop():
    if phase == PRESSING: release(steps[currentStep])
    reset state
```

Macro definitions live in `Macro.h` as `const MacroStep[]` arrays with a
`MacroDef` header (name + pointer + length). Adding a new macro means adding
an array and an entry in the table — no other code changes.

---

### NVS Storage Schema

Namespace: `"clicker"` (Preferences library). All keys ≤ 15 characters.

| Key | Type | Default | Description |
|---|---|---|---|
| `mode` | String | `AutoCast` | Active mode name |
| `ac.leftEn` | Bool | false | AutoCast left enabled |
| `ac.leftMs` | UInt32 | 1600 | AutoCast left interval ms |
| `ac.rightEn` | Bool | false | AutoCast right enabled |
| `ac.rightMs` | UInt32 | 600 | AutoCast right interval ms |
| `ac.pressMs` | UInt32 | 100 | AutoCast press duration ms |
| `ac.saveEn` | Bool | true | AutoCast auto-save enabled |
| `ac.saveSec` | UInt32 | 60 | AutoCast save interval seconds |
| `hb.left` | Bool | false | HoldButton left held state |
| `hb.right` | Bool | false | HoldButton right held state |
| `ks.keys` | String | `3 1` | KeySequence tokens, space-separated |
| `ks.pressMs` | UInt32 | 25 | KeySequence press duration ms |
| `ks.delayMs` | UInt32 | 250 | KeySequence delay between keys ms |
| `mac.index` | UInt32 | 0 | Macro index |
| `wifi.ssid` | String | `""` | WiFi SSID (captive-portal path only) |
| `wifi.pass` | String | `""` | WiFi password (captive-portal path only) |

Params are loaded at boot into each mode's struct. Writes happen immediately
on `setParam` so a power cycle never loses a change.

The running state is **not** saved. The clicker always boots idle; the user
hits Start when ready.

---

### HID Layer API

`HID.h` wraps TinyUSB so mode code never touches the USB library directly.
When `USB_HID_MODE` is not defined, every function is a no-op stub.

```cpp
namespace HID {

// Call once in setup(), after USB.begin()
void begin();

// Keyboard
void pressKey(uint8_t hidCode);    // HID_KEY_* constants from TinyUSB
void releaseKey(uint8_t hidCode);
void tapKey(uint8_t hidCode);      // press + release in one call
void releaseAllKeys();

// Mouse buttons
// Codes: MOUSE_BUTTON_LEFT (0x01), MOUSE_BUTTON_RIGHT (0x02)
void pressMouse(uint8_t button);
void releaseMouse(uint8_t button);
void releaseAllMouse();

// Release everything — called on stop() and emergency reset
void releaseAll();

}  // namespace HID
```

Common HID key constants used in this project:

| Constant | Key |
|---|---|
| `HID_KEY_F5` | F5 (save game) |
| `HID_KEY_ARROW_LEFT/RIGHT/UP/DOWN` | Arrow keys |
| `HID_KEY_RETURN` | Enter |
| `HID_KEY_ESCAPE` | Escape |
| `HID_KEY_SPACE` | Space |
| `'a'` – `'z'`, `'0'` – `'9'` | Use `HID::charToCode(c)` helper |

`tapKey()` is a convenience for the auto-save F5 press — it handles press,
a short internal delay, and release in one call so mode code stays simple.

---

### WebSocket Protocol (Complete)

All messages are JSON. The WebSocket endpoint is `ws://<ip>/ws`.

#### Browser → ESP32

```jsonc
// Request full status (sent automatically by browser on connect)
{ "cmd": "getStatus" }

// Switch active mode — stops any running mode first
{ "cmd": "setMode", "mode": "AutoCast" }

// Start the active mode
{ "cmd": "start" }

// Stop the active mode
{ "cmd": "stop" }

// Update a parameter — effect is immediate if running, always saved to NVS
// value type matches the param (bool, number, or string)
{ "cmd": "setParam", "key": "leftInterval",  "value": 800    }
{ "cmd": "setParam", "key": "leftEnabled",   "value": true   }
{ "cmd": "setParam", "key": "pressDuration", "value": 50     }
{ "cmd": "setParam", "key": "keys",          "value": "3 1"  }
{ "cmd": "setParam", "key": "macroIndex",    "value": 0      }
```

#### ESP32 → Browser

```jsonc
// Full status — sent on connect, mode change, start, stop, and any setParam.
// "params" object contains all fields for the active mode.
{
  "type": "status",
  "mode": "AutoCast",
  "running": false,
  "params": {
    "leftEnabled":   false,
    "leftInterval":  1600,
    "rightEnabled":  false,
    "rightInterval": 600,
    "pressDuration": 100,
    "autoSave":      true,
    "saveInterval":  60
  }
}

// Error — e.g. Start pressed with empty key list
{ "type": "error", "message": "KeySequence: no keys defined" }
```

Full status is sent every 200 ms while running (no separate tick type —
the browser just re-applies the same status render, which is idempotent).
This keeps the browser JS simple: one `onMessage` handler, one render path.

---

### Concurrency Model

`ESPAsyncWebServer` runs its WebSocket callbacks on **Core 0**. `loop()` —
and every mode's `poll()` — runs on **Core 1**. This makes the
`jffordem_Scheduler` library's `MainSchedule` unsuitable here: it is
single-threaded by design, with no mutexes or atomics. Calling `setParam()`
or `start()` from a WebSocket callback while `poll()` is running on the
other core would be an unguarded data race.

The solution is a **FreeRTOS command queue**:

```
Core 0 (WebSocket callback)          Core 1 (loop)
        │                                  │
        │  xQueueSend(cmdQueue, &cmd)      │
        └──────────────────────────────────►│
                                           │  drain queue
                                           │  apply commands to mode state
                                           │  activeMode->poll(now)
                                           │  push WebSocket status
```

WebSocket callbacks only push a `ClickerCmd` struct into the queue — they
never touch mode state directly. `loop()` drains the queue at the top of
each iteration, then calls `poll()`. All mode state lives exclusively on
Core 1.

```cpp
struct ClickerCmd {
    enum Type { SET_MODE, START, STOP, SET_PARAM } type;
    char   paramKey[20];
    char   paramVal[64];   // always string; mode parses to correct type
};

QueueHandle_t cmdQueue;   // created in setup() with capacity 16
```

The queue capacity of 16 is generous — a single WebSocket message produces
at most one command, and `loop()` drains faster than any user can tap.

**What this means for the library:** `MainSchedule` is not used.
`Timer` (from `Clock.hpp`) is used directly inside mode state machines —
it is read-only from the WebSocket side and only ever written by `poll()`
on Core 1, so no locking is needed. `Pressable` / `DummyButton` are the
conceptual model for `HID.h`'s stub behavior when `USB_HID_MODE` is unset.

---

### Startup Sequence

```
setup():
  1. Serial.begin(115200)              ← always on, even in USB_HID_MODE
                                         (uses UART0 via USB-to-Serial chip or
                                          hardware UART pins, not the OTG port)
  2. cmdQueue = xQueueCreate(16, sizeof(ClickerCmd))

  3. Preferences.begin("clicker")
     load all mode params from NVS
     load activeMode name

  4. #ifdef USB_HID_MODE
       USB.begin()
       HID::begin()          ← inits TinyUSB keyboard + mouse
     #endif

  5. WiFi:
       try secrets.h / NVS credentials → WiFi.begin(ssid, pass)
       wait up to 10 s for connection
       if failed → WiFi.softAP("ESP32-Clicker", "clicker1")
       log IP address to Serial

  6. LittleFS.begin(true)              ← true = format if mount fails

  7. WebServer routes:
       GET /        → LittleFS /index.html
       GET /macros  → JSON list of built-in macro names (for UI dropdown)
       WS  /ws      → wsHandler (pushes ClickerCmd to cmdQueue only)

  8. server.begin()

loop():                                ← Core 1
  // Drain command queue first — all state changes happen here
  ClickerCmd cmd;
  while (xQueueReceive(cmdQueue, &cmd, 0) == pdTRUE):
      applyCommand(cmd)

  // Run active mode
  now = millis()
  activeMode->poll(now)

  // Push WebSocket heartbeat
  if (now - lastWsPush >= 200 && ws.count() > 0):
      ws.textAll(activeMode->statusJson())
      lastWsPush = now

  ws.cleanupClients()
```

`applyCommand()` handles `SET_MODE`, `START`, `STOP`, and `SET_PARAM` —
all the transitions that previously appeared in the WebSocket callback.
`statusJson()` returns the full `{ "type":"status", ... }` blob; sending it
every 200 ms also covers the heartbeat, so no separate tick message is needed.

---

### index.html Implementation Notes

Single file, no external dependencies. Target size: under 20 KB uncompressed
(fits comfortably in LittleFS; gzip if needed).

**Structure:**

```
<header>   title + status dot (pulsing animation while running)
<select>   mode dropdown
<section>  one hidden <div> per mode; the active one is shown
<footer>   START / STOP buttons
```

**CSS — dark theme, touch-friendly:**

```css
:root {
  --bg:      #1a1a1a;
  --surface: #2c2c2c;
  --text:    #e0e0e0;
  --green:   #4caf50;
  --red:     #f44336;
  --accent:  #2196f3;
}
body { background: var(--bg); color: var(--text); font: 16px system-ui; }

/* Inputs: large enough for thumbs */
input[type=number], select { min-height: 44px; font-size: 1.1rem; }

/* Toggle: CSS pill, no JS library */
.toggle input { display: none; }
.toggle label  { display: inline-block; width: 56px; height: 28px;
                 background: #555; border-radius: 14px; cursor: pointer; }
.toggle input:checked + label { background: var(--green); }

/* Buttons */
#btn-start { background: var(--green); }
#btn-stop  { background: var(--red);   }
button { width: 100%; padding: 14px; font-size: 1.2rem; border: none;
         border-radius: 8px; color: #fff; cursor: pointer; margin: 6px 0; }
button:disabled { opacity: 0.35; }
```

**JS — WebSocket management:**

```js
let ws, paramTimer;

function connect() {
    ws = new WebSocket('ws://' + location.host + '/ws');
    ws.onopen    = () => ws.send(JSON.stringify({ cmd: 'getStatus' }));
    ws.onmessage = (e) => onMessage(JSON.parse(e.data));
    ws.onclose   = () => setTimeout(connect, 2000);   // auto-reconnect
}

function onMessage(msg) {
    if (msg.type === 'status') applyStatus(msg);
    if (msg.type === 'tick')   setDot(msg.running);
    if (msg.type === 'error')  showBanner(msg.message);
}

// Debounce param changes — don't flood the ESP32 on every keystroke
function onParamChange(key, value) {
    clearTimeout(paramTimer);
    paramTimer = setTimeout(() =>
        ws.send(JSON.stringify({ cmd: 'setParam', key, value })), 300);
}

function applyStatus(s) {
    document.getElementById('mode-select').value = s.mode;
    showParamSection(s.mode);
    setDot(s.running);
    document.getElementById('btn-start').disabled = s.running;
    document.getElementById('btn-stop').disabled  = !s.running;
    populateParams(s.mode, s.params);
}

connect();
```

**Banner for reconnect notice:**

When the WebSocket reconnects after a drop, if the returned status shows
`running: true`, a dismissible banner appears at the top:
`"Clicker is still running — tap STOP to halt."` This satisfies the
safety-stop decision (keep running on disconnect) without silently surprising
the user.
