# ESP32 Morse Trainer — Project Plan

A Morse code copy trainer. The ESP32 flashes a built-in LED (or later drives a buzzer)
with random letters, words, or QSO elements. You copy what you see/hear into a browser
input field. The web UI controls Koch level, speed, and dictionary.

No extra hardware required for Phase 1. A passive buzzer adds audio in Phase 4.

---

## Hardware Notes

**Output — Phase 1–3:** GPIO48 RGB LED (WS2812B, built into ESP32-S3-DevKitC-1).
White on = mark. Off = space. Controlled via Adafruit NeoPixel or FastLED.

**Output — Phase 4 upgrade:** passive buzzer on any free GPIO via `ledcWriteTone()`.
The `MorseOutput` abstraction keeps the engine code unchanged; swap implementation only.

**Input:** WiFi + browser only. No paddle, no key — this is a *receive* trainer.

---

## Morse Timing Reference

Standard PARIS timing at *N* WPM: 1 unit = 1200 / N ms.

| Element  | Duration |
|----------|----------|
| dit      | 1 unit   |
| dah      | 3 units  |
| intra-char gap | 1 unit |
| inter-char gap | 3 units |
| inter-word gap | 7 units |

**Farnsworth speed:** character elements sent at `charWpm`; inter-char and
inter-word gaps stretched so the *effective* word rate is `effectiveWpm`.
Lets beginners copy individual characters before building to full speed.

---

## Koch Progression

Start with K and M. Add one letter at a time in this order (ITU Koch order):

```
K M R S U A P T L O W I . N J E F 0 Y V , G 5 / Q 9 Z H 3 8 B ? 4 2 7 C 1 D 6 X
```

`kochLevel` = number of characters unlocked from the left of this list. Level 2 = K+M.
A session randomly picks characters from the active set until the user can copy ≥90%
correctly at current speed, then they advance.

---

## File Map

```
esp32-morse/
├── CLAUDE.md               (build/flash notes, mirrors clicker pattern)
├── PROJECT-PLAN.md         (this file)
│
│── Phase 1: Skeleton ──────────────────────────────────────────────────────────
├── platformio.ini          two envs: dev (debug) and morse (LED active)
├── flash.bat               convenience wrapper
├── .gitignore
├── src/main.cpp            setup() / loop() / WiFi / queue drain
├── src/secrets.h           WIFI_SSID / WIFI_PASSWORD  [gitignored]
├── src/Timer.h             copy from esp32-clicker (no changes needed)
├── src/MorseCmd.h          MorseCmd struct + QueueHandle_t declaration
├── src/MorseOutput.h       abstraction: LED-only in Phase 1, buzzer added Phase 4
├── src/WebUI.h/.cpp        AsyncWebServer + WebSocket handler
└── data/index.html         placeholder page; replaced in Phase 2

│── Phase 2: Morse Engine ──────────────────────────────────────────────────────
├── src/MorseEngine.h       dit/dah/gap sequencer, Farnsworth timing, drives MorseOutput
└── src/MorseChar.h         character → element sequence table (A-Z, 0-9, punctuation)

│── Phase 3: Koch Trainer ──────────────────────────────────────────────────────
├── src/KochTrainer.h       random char picker, quiz state, score tracking
└── data/index.html         Koch level, WPM, Farnsworth sliders + copy input + score

│── Phase 4: Word & QSO Mode ───────────────────────────────────────────────────
├── src/WordList.h          word lists: CW abbrevs, callsigns, Q-codes, contest exchanges
├── src/WordTrainer.h       random word/group picker; quiz state mirrors KochTrainer
└── data/index.html         +mode selector: Letters / Words / QSO

│── Phase 5: Buzzer ────────────────────────────────────────────────────────────
    MorseOutput.h           add BuzzerOutput subclass (ledcWriteTone on configurable GPIO)
    data/index.html         +tone frequency slider; +output selector LED/Buzzer

│── Phase 6: Polish ────────────────────────────────────────────────────────────
    NVS persistence         save kochLevel, wpm, farnsworth, lastMode across power cycles
    WiFi AP fallback        captive portal if no credentials saved
    Progress chart          session history (% correct per speed/level) in browser
```

---

## Phase 1 — Skeleton

**Goal:** Compiles, connects to WiFi, serves placeholder page, WebSocket opens.
LED blink test confirms GPIO48 control works. No Morse yet.

### `platformio.ini`

```ini
[platformio]
default_envs = dev

[env]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
board_build.mcu = esp32s3
board_build.flash_size = 16MB
board_build.flash_mode = qio
board_build.filesystem = littlefs
monitor_speed = 115200
lib_deps =
    mathieucarbou/AsyncTCP
    mathieucarbou/ESPAsyncWebServer
    bblanchon/ArduinoJson @ ^7
    adafruit/Adafruit NeoPixel @ ^1

[env:dev]
monitor_port = COM5
build_flags =
    -DARDUINO_USB_MODE=1

[env:morse]
monitor_port = COM5
upload_port = COM5
upload_protocol = esptool
upload_speed = 921600
build_flags =
    -DARDUINO_USB_MODE=1
    -DMORSE_ACTIVE
```

The `morse` env is the normal deploy target (no HID complexity here).
`MORSE_ACTIVE` gates LED initialization so the NeoPixel isn't driven in dev
mode if GPIO48 conflicts with anything during debugging.

### `MorseCmd.h`

```cpp
struct MorseCmd {
    enum Type { SET_PARAM, START, STOP } type;
    char key[24];
    char val[64];
};
extern QueueHandle_t cmdQueue;
```

### `MorseOutput.h`

```cpp
class MorseOutput {
public:
    virtual void mark() = 0;   // sound/light on
    virtual void space() = 0;  // sound/light off
    virtual void begin() = 0;
};

class LedOutput : public MorseOutput {
    // Adafruit_NeoPixel on GPIO48, index 0, white at ~30 brightness
public:
    void begin() override;
    void mark() override;
    void space() override;
};
```

### Done When

- Compiles
- WiFi connects, IP on Serial
- Browser at `http://<ip>/` gets placeholder HTML
- WebSocket connects (devtools confirm)
- LED blinks white 3× on boot as hardware self-test
- No crash after 5 minutes idle

---

## Phase 2 — Morse Engine

**Goal:** The ESP32 can send any string as Morse via the LED. Speed, Farnsworth
speed are configurable. Sending is non-blocking (`poll()` based, not `delay()`).

### `MorseChar.h`

Encode each character as a null-terminated string of `.` and `-` characters.
Store as a lookup table indexed by ASCII value (space for unsupported chars).

```cpp
const char* morseEncode(char c);
// morseEncode('A') → ".-"
// morseEncode('K') → "-.-"
// morseEncode(' ') → nullptr (caller inserts word gap)
```

### `MorseEngine.h`

Non-blocking state machine. `poll(uint32_t now)` called from `loop()` each cycle.

States:
```
IDLE → MARK → INTRA_GAP → (next element or) CHAR_GAP → WORD_GAP → IDLE
```

Key parameters:
- `charWpm` — speed for individual character elements (default 20)
- `effectiveWpm` — overall rate including gaps (default 10, ≤ charWpm)
- `send(const char* text)` — queue a string to transmit; returns immediately

Farnsworth gap calculation:
```
unitMs = 1200 / charWpm
charGapMs = max(3 * unitMs, (50 * 1000/effectiveWpm - 31 * unitMs) / 19)
wordGapMs = 7.0/3.0 * charGapMs  (maintain ratio)
```

### Done When

- `engine.send("CQ")` flashes `CQ` correctly on the LED
- Timing at 20/10 Farnsworth matches a reference decoder
- Changing WPM mid-session takes effect on the next character
- `stop()` halts mid-transmission and turns LED off

---

## Phase 3 — Koch Trainer

**Goal:** The trainer runs a session: sends random characters from the active
Koch set, waits for user input via WebSocket, scores the response.

### Session Flow

```
START received
  → pick random char from kochSet[0..kochLevel]
  → engine.send(char)
  → engine finishes → wait 2s → send next char
  → user types in browser, ENTER submits
  → server compares input to last sent char (trim, case-fold)
  → push score update via WebSocket
STOP received → engine.stop() → IDLE
```

### `KochTrainer.h`

```cpp
class KochTrainer {
public:
    void setLevel(int level);          // 2–42
    void setCharWpm(int wpm);
    void setEffectiveWpm(int wpm);
    void start();
    void stop();
    void poll(uint32_t now);
    void submitAnswer(const char* input);
    String statusJson() const;
};
```

Score fields in `statusJson()`:
```json
{
  "running": true,
  "kochLevel": 5,
  "charWpm": 20,
  "effectiveWpm": 10,
  "lastSent": "A",
  "correct": 14,
  "total": 17,
  "pct": 82
}
```

### `data/index.html`

- Koch level slider (2–42); label shows active character set as it moves
- Char WPM slider (10–30); Effective WPM slider (5–25, capped ≤ charWpm)
- Large START / STOP button
- Status area: last sent character revealed after user submits (or after 5s timeout)
- Text input for copy; submits on Enter or after 5s auto-reveal
- Running score: `14 / 17 (82%)`

### Done When

- Random Koch letters flash; user can type copy; score updates
- Farnsworth controls change timing without restart
- Level slider changes the active character set immediately
- Score resets on each Start

---

## Phase 4 — Word & QSO Mode

**Goal:** Send complete words or contest/QSO elements rather than individual
characters. Good for building head copy after Koch letters are solid.

### `WordList.h`

Static arrays:
- `CW_ABBREVS[]` — 73, 88, CQ, DE, BK, AR, SK, PSE, TNX, RST, etc.
- `CALLSIGNS[]` — typical North American patterns (KG7FVO style)
- `QCODES[]` — QRM, QSB, QSY, QTH, QRN, QRP, QRO, QRQ, QRS, QRZ
- `CONTEST_EXCHANGES[]` — "5NN WA", "5NN OR", "5NN CA" etc. (RST + state)

### Done When

- Mode selector switches between Letters, Words, QSO
- Each word/group sends as a unit with proper inter-word spacing
- Copy box accepts multi-character input for words

---

## Phase 5 — Buzzer

**Goal:** Add audio output. LED trainer is silent and strains the eyes; tone
output is far more effective for real CW practice.

Hardware: passive buzzer on any free GPIO, driven via `ledcWriteTone()`.

### Changes

- `MorseOutput.h`: add `BuzzerOutput` implementing `mark()`/`space()` via LEDC
- `main.cpp`: instantiate `LedOutput`, `BuzzerOutput`, or both
- Web UI: tone frequency slider (400–900 Hz); output selector (LED / Buzzer / Both)
- `platformio.ini`: no new libraries needed; LEDC is in arduino-esp32 core

---

## Phase 6 — Polish

**Goal:** Self-contained appliance. Settings survive reboots. Progress visible over time.

- **NVS** — save kochLevel, wpm, effectiveWpm, lastMode on every param change
- **WiFi AP fallback** — same captive-portal pattern as clicker Phase 6
- **Progress history** — LittleFS JSON log of session results (date, level, wpm, pct);
  browser shows a simple chart (Chart.js from CDN or hand-rolled SVG)

---

## Testing Strategy

Phase 1–2: confirm timing with serial output. Log each mark/space interval in `poll()`:
```
[1200] MARK 96ms (dit)
[1296] SPACE 96ms (intra)
[1392] MARK 288ms (dah)
...
```
At 20 WPM, 1 unit = 60 ms. Verify against that.

Phase 3+: test scoring logic with known inputs before worrying about LED timing.

---

## Known Risks

**NeoPixel latency** — WS2812 transmission (~30 µs for 1 LED) is negligible at Morse
speeds, but avoid calling `show()` from an ISR. The Phase 2 state machine calls it only
from `loop()`, which is safe.

**Farnsworth math** — The standard formula for Farnsworth gaps is easy to get subtly
wrong. Cross-check a few characters against [lcwo.net](https://lcwo.net) or a phone
CW decoder app before declaring Phase 2 done.

**WebSocket timing** — Sending status JSON every 200 ms from `loop()` is fine. Do not
send a status push from inside the engine's `poll()` on the same call stack — push only
from `loop()` to keep the WebSocket handler off the engine's hot path.
