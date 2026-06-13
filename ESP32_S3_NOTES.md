# ESP32-S3 N16R8 — Plans & Notes

Hardware: 3x ESP32-S3 N16R8 (16MB Flash, 8MB OPI PSRAM, dual-core 240MHz, native USB, WiFi, BLE 5.0)

---

## First Board Bringup

1. Add board manager URL in Arduino IDE or PlatformIO:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
2. Install **esp32 by Espressif Systems** (v3.x)
3. Select board: **ESP32S3 Dev Module**
4. Set in Tools menu:
   - USB Mode: **USB-OTG (TinyUSB)**  ← required for HID
   - PSRAM: **OPI PSRAM**             ← activates the 8MB
   - Flash Size: **16MB**
   - Partition Scheme: **16M Flash (3MB APP/9MB FATFS)**
5. Upload a Blink sketch to verify before loading the library

PlatformIO equivalent (`platformio.ini`):
```ini
[env:esp32s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
board_build.flash_size = 16MB
board_build.partitions = default_16MB.csv
board_upload.flash_size = 16MB
build_flags =
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_MODE=0
    -DARDUINO_USB_CDC_ON_BOOT=0
lib_deps =
    file://./
```

---

## Use Case 1: HID Gaming (Skyrim, Subnautica)

**Library change needed: none.** HIDIO.hpp works as-is.

Sketch-level change — replace Leonardo includes with:
```cpp
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <USBHIDMouse.h>

USBHIDKeyboard Keyboard;
USBHIDMouse Mouse;

void setup() {
    USB.begin();
    Keyboard.begin();
    Mouse.begin();
    schedule.begin();
}
```

---

## Use Case 2: Cat Box Stepper Motor Web Appliance

Rough architecture:
```
Core 0: WiFi + WebServer (handleClient loop)
Core 1: MainSchedule + AccelStepper (your existing pattern)
```

Sketch skeleton:
```cpp
#include <WiFi.h>
#include <WebServer.h>
#include <AccelStepper.h>
#include <Scheduler.hpp>

const char* SSID = "...";
const char* PASSWORD = "...";

WebServer server(80);
MainSchedule schedule;

// your scheduler objects here...

void webTask(void *) {
    server.on("/", handleRoot);
    server.on("/pour", handlePour);
    server.begin();
    for (;;) { server.handleClient(); vTaskDelay(1); }
}

void setup() {
    Serial.begin(115200);
    WiFi.begin(SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED) delay(500);
    Serial.println(WiFi.localIP());

    EEPROM.begin(64);  // required on ESP32 before Persistent.hpp reads

    xTaskCreatePinnedToCore(webTask, "web", 4096, NULL, 1, NULL, 0);
    schedule.begin();
}

void loop() {
    schedule.poll();
}
```

Libraries needed (Library Manager):
- **AccelStepper** — stepper motor control
- **ESPAsyncWebServer** — optional, better for non-blocking web (install with AsyncTCP)

---

## Use Case 3: Classic Text Adventures (Z-machine / Zork)

The 8MB PSRAM can hold the entire Z-machine state + large story files in RAM.

Story files (.z5, .z8) live in **LittleFS** (16MB Flash — room for dozens of games).

Existing MicroZork example is the starting point. Upgrade path:
1. Add LittleFS support: `#include <LittleFS.h>`
2. Upload story files via Arduino IDE "Upload Filesystem Image" or PlatformIO `pio run -t uploadfs`
3. Integrate a Z-machine interpreter (frotz-esp32, or IF-engine on GitHub) to run actual Infocom .z5 files
4. Display via existing DisplayBuffer + MenuUI on character LCD, or via web browser over WiFi

PSRAM allocation for large buffers:
```cpp
uint8_t *storyData = (uint8_t*)ps_malloc(512 * 1024);  // allocate from PSRAM
```

---

## Library Compatibility Notes

| Module | Status | Notes |
|--------|--------|-------|
| Scheduler.hpp | Works as-is | |
| Clock.hpp | Works as-is | |
| ButtonHandler.hpp | Works as-is | |
| EncoderWheel.hpp | Works as-is | Can upgrade to interrupt-driven for better reliability |
| Led.hpp | Works as-is | |
| Display.hpp / MenuUI.hpp | Works as-is | |
| HIDIO.hpp | Works as-is | Sketch includes/declarations change (see Use Case 1) |
| Persistent.hpp | Works, one fix | Add `EEPROM.begin(size)` in sketch setup() |
| KeypadHandler.hpp | Works as-is | |
| LK204_25.hpp | Works as-is | I2C/Wire is identical |

### Optional Upgrade: Interrupt-driven EncoderWheel

ESP32-S3 supports hardware interrupts on any GPIO. For fast encoder rotation, attaching interrupts is more reliable than polling. The existing polling approach will still work fine for most use cases.

---

## Custom PCB Fabrication (Future Exploration)

For dedicated appliances (cat box, cookie clicker, etc.) it may be worth designing small custom PCBs rather than using breadboards or perfboard. Chinese fabs offer very low-cost small runs.

**Common fabricators to explore:**
- **JLCPCB** — most popular for hobbyists; 5 boards for ~$2 + shipping, fast turnaround (~1 week to US)
- **PCBWay** — similar pricing, also offers assembly (PCBA) where they source and solder components
- **LCSC** — component supplier affiliated with JLCPCB; can order parts alongside PCB

**Design tools to explore:**
- **KiCad** — open-source, industry-standard schematic + PCB layout. Exports Gerber files (what fabs want). Large community, good ESP32 footprint libraries available.
- **EasyEDA** — browser-based, tightly integrated with JLCPCB/LCSC. Lower learning curve, good for simple boards.

**What a custom board buys you:** a permanent, compact form factor with the ESP32 module, voltage regulation, connectors for stepper/sensors, and status LED all in one — no more rats-nest wiring for a deployed appliance.

**TODO:** Explore KiCad or EasyEDA when ready to commit a design (e.g. cat box controller board).

---

## Cat Box Controller Board — Draft Component List

A first-pass bill of materials for a dedicated PCB:

1. **Power input:** 2-position screw terminal lugs (5mm pitch) — strip/tin supply wires and screw in directly; no barrel jack compatibility issues, works with any 12V supply
2. **Buck converter:** MP2307 or LM2596 (+ inductor + caps) — 12V → 3.3V for ESP32; designed into the PCB rather than a plug-in module
3. **ESP32-S3 module:** castellated-edge footprint (e.g. ESP32-S3-WROOM-1) soldered directly, or pin-header socket for removability
4. **Stepper driver:** StepStick/Pololu-style 2×8 socket — accepts A4988, DRV8825, or TMC2209 interchangeably; **TMC2209 recommended** for quiet operation
5. **Stepper output:** 4-position screw terminal (one per motor coil pair)
6. **Interface signals:** STEP, DIR, ENABLE from ESP32 GPIO → stepper socket (3 traces)
7. **Extras:** status LED + resistor, reset button, optional 4-pin debug/UART header

This is a simple 2-layer board — well within a first KiCad project.

### Stepper Driver Comparison (StepStick footprint)

| Driver | Current | Max V | Notes |
|--------|---------|-------|-------|
| TMC2209 | 2A | 29V | Quiet (StealthChop), UART config, ~$3–5 — **recommended** |
| DRV8825 | 2.2A | 45V | Solid, simple, ~$1–2 |
| A4988 | 2A | 35V | Classic, cheapest, louder |

---

## Display Hardware (Optional Enhancement)

Selected: **Gotor 2.8" ILI9341 TFT LCD Touch Display, SPI, ~$9.99** (Amazon wishlist)

- **Display driver:** ILI9341 (240×320, SPI)
- **Touch controller:** XPT2046 (resistive touch) — typical for this module
- **Logic level:** 3.3V compatible

### Libraries
- **TFT_eSPI** — recommended display driver for ESP32 + ILI9341. Requires editing `User_Setup.h` to set chip type (ILI9341) and SPI pin assignments before first compile.
- **XPT2046_Touchscreen** (Paul Stoffregen) — pairs with TFT_eSPI for touch input.
- Adafruit ILI9341 + Adafruit GFX also works if you prefer the Adafruit ecosystem (consistent with `Graphics.hpp`).

### Quick wiring (typical ESP32-S3 SPI pins)
| Display Pin | ESP32-S3 |
|-------------|----------|
| VCC | 3.3V |
| GND | GND |
| CS | GPIO 10 |
| RESET | GPIO 9 |
| DC/RS | GPIO 8 |
| MOSI (SDA) | GPIO 11 |
| SCK | GPIO 12 |
| LED | 3.3V (or PWM pin for brightness control) |
| T_CS | GPIO 13 (touch chip select) |
| T_CLK | GPIO 12 (shared SPI clock) |
| T_MOSI | GPIO 11 (shared) |
| T_MISO | GPIO 14 |

*(Pin numbers are suggestions — ESP32-S3 SPI is flexible, configure to match in `User_Setup.h`)*

### Display as Diagnostic Panel

When a display is attached, use it as a boot/status panel — especially useful when the web UI isn't reachable because WiFi hasn't connected yet:

```
Connecting to WiFi...
Connected!
IP: 192.168.1.42
http://catbox.local
```

Suggested boot sequence to show on screen:
1. "Connecting to WiFi..." while `WiFi.status() != WL_CONNECTED`
2. "Connected!" + IP address + mDNS hostname once up
3. Steady-state: show device status (motor state, last action, uptime, etc.)

This keeps the web UI as the primary interface while giving a local fallback for "why can't I reach it?"

---

## Design Philosophy: Headless-First

The baseline config is a **headless IoT appliance** — no display, no buttons beyond reset. The web UI (via phone browser) is the primary interface, not an optional extra. LCD displays (2- or 4-line character) may be added later as a convenience, but the web UI must work standalone.

Implications:
- All status and control must be accessible via the web interface
- mDNS discovery is essential, not optional (no screen to print the IP)
- Serial output is dev/debug only — not a user-facing channel in deployed hardware
- When a display *is* attached, its primary role is a **diagnostic panel** answering the two hardest headless failure modes: "did it boot?" (screen lights up) and "did it connect to WiFi?" (IP + hostname visible). Control stays in the web UI.

---

## Device Discovery (mDNS / .local hostnames)

The ESP32 gets a DHCP-assigned IP — the phone has no way to know it. Fix: advertise via **mDNS** (Multicast DNS / Bonjour), so the phone can reach it by name.

```cpp
#include <WiFi.h>
#include <ESPmDNS.h>

// In setup(), after WiFi connects:
if (MDNS.begin("catbox")) {
    Serial.println("mDNS: http://catbox.local");
}
```

Phone browser navigates to `http://catbox.local` — no IP lookup needed.

**Per-device hostnames** (3 boards → 3 names):
| Board | Hostname |
|-------|----------|
| Cat box appliance | `catbox.local` |
| Skyrim/HID gaming | `skyrim.local` |
| Z-machine / Zork | `zork.local` |

**Platform support:** iOS — native (Bonjour built in). Android — Chrome resolves `.local` since Android 12; works on older Android too via Chrome's mDNS resolver. Works on home/office networks; may be blocked on guest/hotel WiFi (fall back to IP in that case).

**Fallbacks for IP discovery:**
- `Serial.println(WiFi.localIP())` — visible in Arduino Serial Monitor during dev
- Print IP on character LCD at boot if one is attached

---

## Web UI Design: Smartphone-First

All web interfaces should be usable from a phone browser — no pinch-zoom required, large touch targets, minimal typing.

### Practical guidelines

- Use a responsive `<meta viewport>` tag:
  ```html
  <meta name="viewport" content="width=device-width, initial-scale=1">
  ```
- **Large buttons**: minimum 48×48 px tap targets (Material Design / Apple HIG recommendation). For cat-box or Skyrim controls, full-width buttons are ideal.
- **Minimal text input**: prefer toggles, sliders, and buttons over text fields. Use `<input type="number">` with `min`/`max` when numeric entry is needed.
- **Single-page, low-JS**: serve a self-contained HTML page from `WebServer` or `ESPAsyncWebServer`. Avoid external CDN dependencies — the ESP32 is on a local network.
- **Auto-refresh or fetch polling**: use `fetch("/status")` on a short interval (e.g. 2 s) to update status without a page reload:
  ```js
  setInterval(() => fetch('/status').then(r=>r.json()).then(updateUI), 2000);
  ```
- **Dark mode friendly**: use `@media (prefers-color-scheme: dark)` or a dark default — phones often have it enabled.

### Minimal responsive skeleton

```cpp
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  body { font-family: sans-serif; margin: 0; padding: 1rem; background:#111; color:#eee; }
  button { display:block; width:100%; padding:1rem; margin:.5rem 0;
           font-size:1.2rem; border:none; border-radius:.5rem;
           background:#1e88e5; color:#fff; cursor:pointer; }
  button:active { background:#1565c0; }
</style></head><body>
<h2>Cat Box</h2>
<button onclick="fetch('/pour')">Pour Litter</button>
<button onclick="fetch('/status').then(r=>r.text()).then(s=>document.getElementById('st').innerText=s)">Refresh Status</button>
<p id="st">–</p>
</body></html>
)rawliteral";
```

---

## WiFi / BLE Quick Reference

### WiFi
```cpp
#include <WiFi.h>
WiFi.begin("SSID", "password");
while (WiFi.status() != WL_CONNECTED) delay(500);
Serial.println(WiFi.localIP());
```

### Bluetooth Classic Serial (not available on S3 — S3 has BLE only)
Use BLE instead:
```cpp
#include <BLEDevice.h>
// Use NimBLE-Arduino library for lighter weight BLE
```

### BLE HID (wireless keyboard/mouse — alternative to USB HID)
Libraries: **ESP32-BLE-HID** or **NimBLE-Arduino**
Useful if you want a wireless HID device instead of wired USB.
