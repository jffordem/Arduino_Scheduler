# Arduino Scheduler Library

Composable abstractions for Arduino development. Build complex behaviors by combining small, focused classes — inputs (buttons, encoders, keypads), outputs (LEDs, HID keyboard/mouse), display/menu systems, and a central polling scheduler that ties them together.

**License:** MIT

## Quick Start

Every sketch follows the same three-line pattern:

```cpp
MainSchedule schedule;
// ... declare components, passing schedule as first argument ...
void setup() { schedule.begin(); }
void loop()  { schedule.poll(); }
```

Passing `schedule` to a component automatically registers it for polling. You never manually add things to the loop.

## Examples

### Blinky — LED + Clock

Compose `Clock` and `DigitalLED` to blink without `delay()`:

```cpp
#include <Scheduler.hpp>
#include <Clock.hpp>
#include <Led.hpp>

class Blinky : public Clock, private DigitalLED {
  bool _ledState;
public:
  Blinky(Schedule &s, long &offTime, long &onTime, int pin) :
    Clock(s, offTime, onTime, _ledState),
    DigitalLED(s, _ledState, pin),
    _ledState(LOW) { }
};

long onTime = 100, offTime = 200;
MainSchedule schedule;
Blinky blinky(schedule, offTime, onTime, LED_BUILTIN);
void setup() { schedule.begin(); }
void loop()  { schedule.poll(); }
```

---

### Button → HID Keyboard

Press a button, send a keystroke. Hardware polarity is handled by `lowIsPressed`:

```cpp
#include <Scheduler.hpp>
#include <ButtonHandler.hpp>
#include <HIDIO.hpp>

MainSchedule schedule;
KeyPress escKey(KEY_ESC);
Button btn(schedule, 7, /*lowIsPressed=*/true, escKey);
void setup() { schedule.begin(); }
void loop()  { schedule.poll(); }
```

Use `ButtonHandler` instead of `Button` when you need custom press/release callbacks:

```cpp
void onPress()   { /* ... */ }
void onRelease() { /* ... */ }
ButtonHandler btn(schedule, 7, &onPress, &onRelease);
```

---

### Encoder → Value

Map encoder rotation to a `long` value clamped to a range:

```cpp
#include <Scheduler.hpp>
#include <EncoderWheel.hpp>

long speed = 50;  // starts at midpoint
MainSchedule schedule;
EncoderControl<long> encoder(schedule, {clockPin, dataPin}, speed, /*min=*/0, /*max=*/100);
void setup() { schedule.begin(); }
void loop()  { schedule.poll(); }
```

---

### Character LCD Menu

A complete retained-mode menu for a 4×20 character LCD. Uses `Display.hpp`, `MenuUI.hpp`, and `LK204_25.hpp`:

```cpp
#include <Wire.h>
#include <Scheduler.hpp>
#include <Display.hpp>
#include <MenuUI.hpp>
#include <KeypadHandler.hpp>
#include <LK204_25.hpp>

MainSchedule schedule;
LK204_25_LCD lcd;
LK204_25_Keypad keypad;

long brightness = 50;

void resetBrightness(MenuContext &) { brightness = 50; }

MenuItem items[] = {
    MenuItem::EditLong("Brightness", brightness, /*step=*/5),
    MenuItem::Action("Reset", &resetBrightness),
};
MenuScreen root("Settings", items, 2);

MenuContext menu(root);
MenuRenderer<LK204_25_LCD, 4, 20> renderer(menu);
MainDisplay<LK204_25_LCD, 4, 20> display(schedule, lcd, renderer, /*refreshMs=*/125);

MenuKeymap keymap = {
    /*up*/ KeypadKey_2, /*down*/ KeypadKey_8,
    /*back*/ KeypadKey_D, /*select*/ KeypadKey_A,
    /*line1..4*/ KeypadKey_A, KeypadKey_B, KeypadKey_C, KeypadKey_D
};
MenuKeypadController menuKeys(menu, keymap);
KeypadHandler keypadHandler(schedule, keypad, menuKeys);

void setup() { keypad.begin(); display.begin(); schedule.begin(); }
void loop()  { schedule.poll(); }
```

`MainDisplay` does smart incremental flushing — only changed characters are written to the LCD, with a full refresh every 5 seconds (configurable).

**Menu item types:**

| Factory method | What it does |
|---|---|
| `MenuItem::Action("label", fn)` | Calls `fn(ctx)` on select |
| `MenuItem::Submenu("label", &screen)` | Navigates into another `MenuScreen` |
| `MenuItem::Toggle("label", enabled)` | Calls `toggle()` on an `Enabled` object; shows `*` when on |
| `MenuItem::EditLong("label", value, step)` | Adjust a `long` with up/down keys |
| `MenuItem::EnterLong("label", value)` | Type a number with the keypad (`*`=backspace, `#`=confirm) |
| `MenuItem::EnterString("label", buf, maxLen)` | T9-style text entry |

See `examples/HiLoGame/` for a full working game built with this system.

---

### Displaying Values on Screen

Use `DisplayValue` to show live data without a full menu:

```cpp
long score = 0;
DisplayValue<LK204_25_LCD, long, 4, 20> scoreDisplay(/*row=*/0, /*col=*/0, "Score: ", score, "");
```

---

## Modules

```
Arduino.hpp         — Core shim for editor/lint support
LinkedList.hpp      — List<T>, Enumerable<T>, countZ()
Scheduler.hpp       — Poller, Pressable, Enabled, Composite, MainSchedule
Clock.hpp           — Timer, Clock, PeriodicTrigger, SpeedTest
PinIO.hpp           — DigitalRead, DigitalWrite, AnalogRead, AnalogWrite
EdgeDetector.hpp    — EdgeDetector, Trigger, Counter, FrequencyDivider
Mapper.hpp          — Mapper, Inverter, Constrain, AndInputs, OrInputs, Chooser
HIDIO.hpp           — KeyPress, MouseButton, ButtonController, ValuePresser
Led.hpp             — DigitalLED, SevenSegLED, Pot
ButtonHandler.hpp   — Button, ButtonHandler, ToggleButton, ActiveBuzzer, PassiveBuzzer
EncoderWheel.hpp    — EncoderWheel, EncoderControl
KeypadHandler.hpp   — KeypadHandler, KeypadKeyHandler, ToggleKeypadKeyHandler
Display.hpp         — DisplayBuffer, MainDisplay, DisplayLabel, DisplayValue, Spinner
MenuUI.hpp          — MenuItem, MenuScreen, MenuContext, MenuRenderer, MenuKeypadController
LK204_25.hpp        — LK204_25_LCD, LK204_25_Keypad  (I2C character LCD + keypad)
Graphics.hpp        — Drawable, DrawableComposite, MainWindow, VirtualLED
SerialPlot.hpp      — SerialPlot, PlotBool, PlotNum  (real-time serial debug)
BreadboardConfig.hpp / LeonardoConfig.hpp — Pre-wired pin configurations
```

## Design Notes

- **Polling over interrupts.** All detection is synchronous and deterministic. Default max pollers: 50 (change `MAX_POLLERS` in `Scheduler.hpp`).
- **Hardware abstraction via config flags.** `ButtonConfig::lowIsPressed` and `LedConfig::lowIsOn` handle active-high vs active-low hardware without conditional logic in your code.
- **Header-only.** Include only what you need; unused modules cost nothing.
- Enable the `DEBUG` macro in `Arduino.hpp` to activate serial output. Use `SerialPlot` for real-time signal visualization.
