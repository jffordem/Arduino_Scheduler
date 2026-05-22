# Arduino Scheduler Library - AI Agent Guide

## Project Overview
This is a C++ header library for Arduino microcontroller development. It provides composable abstractions for input devices (buttons, encoders), output devices (LEDs, keyboard/mouse), and a central polling scheduler that coordinates them all.

**Key Design Philosophy:** Black-box composition. Objects are easy to compose and repeat, forming increasingly complex units while hiding implementation details.

## Architecture

### Core Patterns

#### 1. **Scheduler Pattern** (Primary Design)
- `MainSchedule`: Central polling loop coordinator
- All input/output handlers inherit from `Scheduled` and auto-register themselves
- All pollers must take `Schedule &schedule` as first parameter
- **Important:** In `void setup()`, call `schedule.begin()`. In `void loop()`, call `schedule.poll()`.
- Max pollers: 50 (configurable via `MAX_POLLERS`)

#### 2. **Polling Abstractions** (Scheduler.hpp)
```
Poller → Objects that need polling (poll() method)
Pressable → Objects that can be pressed/released
Enabled → Objects that can be enabled/disabled
Composite<T> → Generic container for list-based composition
```

#### 3. **Composition over Inheritance**
- Use `public` inheritance for composites (e.g., `class Blinky : public Clock, private DigitalLED`)
- Private inheritance to hide implementation details
- See examples for composition patterns

### Dependency Tree

```
Arduino.hpp (core shim)
├── LinkedList.hpp (data structures)
└── Scheduler.hpp (fundamental patterns)
    ├── Clock.hpp (timing)
    ├── PinIO.hpp (pin read/write)
    ├── EdgeDetector.hpp (rising/falling edge detection)
    ├── HIDIO.hpp (keyboard/mouse control)
    ├── Mapper.hpp (value transformations)
    ├── Led.hpp → PinIO + Mapper
    ├── ButtonHandler.hpp → EdgeDetector + PinIO + Mapper
    ├── EncoderWheel.hpp → PinIO + EdgeDetector + Mapper + SerialPlot
    ├── SerialPlot.hpp (debug visualization)
    └── Graphics.hpp (display drawing)
```

## Module Reference

### Input Devices

#### **ButtonHandler.hpp**
- `ButtonValue`: Normalizes button signal (HIGH when pressed, regardless of hardware)
- `ButtonHandler`: Calls custom handlers on press/release
- `Button`: Routes button events to `Pressable` object
- `ToggleButton`: Toggles a `Pressable` on each press
- `ButtonConfig`: Configuration struct (pin, lowIsPressed flag)

Usage: `ButtonHandler btn(schedule, btnPin, &onPress, &onRelease);`

#### **EncoderWheel.hpp**
- `EncoderWheel`: Tracks absolute position
- `EncoderControl`: Maps encoder rotation to value range (starts at midpoint)
- `EncoderWheelHandler`: Base class for custom encoder behavior
- `EncoderConfig`: Configuration struct (clockPin, dataPin)

### Output Devices

#### **Led.hpp**
- `DigitalLED`: GPIO-driven LED (normalizes active-high vs active-low)
- `SevenSegLED`: 7-segment display control
- `Pot`: Analog read with mapping
- `LedConfig`: Configuration struct (pin, lowIsOn flag)

#### **HIDIO.hpp**
- `KeyPress`: Presses/releases a keyboard key
- `MouseButton`: Controls mouse buttons (LEFT, RIGHT, MIDDLE)
- `ValuePresser`: Monitors a bool value and presses/releases on changes
- `PressHandler`: Custom press/release handlers via function pointers
- `ButtonController`: Repeating button press (hold time, repeat interval)

### Utilities

#### **Clock.hpp**
- `Timer`: Expirable timer (check with `expired()`, reset with `reset()`)
- `Clock`: Square-wave signal generator with HIGH/LOW times
- `PeriodicBase`/`PeriodicTrigger`: Periodic event firing

#### **EdgeDetector.hpp**
- `EdgeDetector`: Detects rising/falling edges on boolean signals
- `Trigger`: Abstract event fire interface
- `TriggerFunction`: Function pointer trigger wrapper
- `Counter`: Count edges
- `FrequencyDivider`: Divide event frequency
- `PeriodicTrigger`: Fire at regular intervals

#### **Mapper.hpp**
- `Mapper`: Maps input range to output range (wraps `map()`)
- `Inverter`: Inverts boolean signals
- `AndInputs`/`OrInputs`: Boolean logic
- `Constrain`: Constrains values to range
- `Chooser`: Index-based selection from array

#### **LinkedList.hpp**
- `List<T>`: Linked list implementation
- `Enumerable<T>`: Iterator interface
- `countZ()`: Count null-terminated array elements

### Display & Debug

#### **Graphics.hpp**
- `Drawable`: Abstract drawable object
- `DrawableComposite`: Container for drawing multiple objects
- `MainWindow`: Main display handler
- `VirtualLED`: Drawable LED for display

#### **SerialPlot.hpp**
- `SerialPlot`: Serial plotter for debugging
- `PlotBool`/`PlotNum`: Plot specific types
- Use for real-time data visualization during development

### Configuration Files

Platform-specific configurations group pins and device configs:
- `BreadboardConfig.hpp`: Pro Micro breadboard layout
- `LeonardoConfig.hpp`: Arduino Leonardo pinout
- `MinimaConfig.hpp`: Minimal configuration
- Provides `Config.Left.Button`, `Config.Right.Encoder`, etc.

## Common Patterns

### Blinky (LED + Clock Composition)
```cpp
class Blinky : public Clock, private DigitalLED {
  bool _ledState;
public:
  Blinky(Schedule &s, long &offTime, long &onTime, int pin) :
    Clock(s, offTime, onTime, _ledState),
    DigitalLED(s, _ledState, pin),
    _ledState(LOW) { }
};
```

### Encoder-Controlled Value
```cpp
long value = 250;
EncoderControl<long> encoder(schedule, Config.Left.Encoder, value, minVal, maxVal);
```

### Button Triggering HID Action
```cpp
KeyPress escape(KEY_ESC);
Button btn(schedule, btnPin, true, escape);
```

## Development Guidelines

### When Adding Features
1. **Consider polling frequency**: Heavy computations in `poll()` slow everything down
2. **Use compositions**: Combine existing abstractions rather than creating monolithic classes
3. **Pass schedule as first parameter**: Required for auto-registration
4. **Normalize hardware differences**: Use config flags (lowIsPressed, lowIsOn) to abstract hardware variations
5. **Avoid dynamic allocation**: Pre-allocate arrays; respect MAX_POLLERS limit

### When Debugging
1. Enable `DEBUG` macro in Arduino.hpp for serial output
2. Use `SerialPlot` for real-time signal visualization
3. Chain `poll()` calls: fewer large polls better than many small ones
4. Check edge detector firing: verify `onRisingEdge()`/`onFallingEdge()` order

### Build & Test
- No separate build system; compile as normal Arduino sketch
- Include only needed headers to minimize compilation time
- Platform configs reduce boilerplate for common hardware setups
- Examples in `examples/` folder demonstrate each module

## Key Files by Purpose

| Purpose | File |
|---------|------|
| Start here | Scheduler.hpp, Clock.hpp, Led.hpp |
| Button input | ButtonHandler.hpp, EdgeDetector.hpp |
| Encoder input | EncoderWheel.hpp, Mapper.hpp |
| Keyboard/Mouse | HIDIO.hpp |
| Value transforms | Mapper.hpp |
| Display graphics | Graphics.hpp |
| Real-time debug | SerialPlot.hpp |
| Arduino shim | Arduino.hpp |

## Design Principles

1. **Composition over Inheritance**: Prefer composition; use inheritance for interface implementation
2. **Pass schedule to all pollers**: Ensures visibility and auto-registration
3. **Hardware abstraction via config**: Use config structs with flags to normalize differences
4. **Black-box abstractions**: Hide complexity behind simple interfaces
5. **Type safety with templates**: Use templates for Mapper, AnalogRead, etc. without RTTI
6. **Polling over interrupts**: Simpler reasoning, deterministic polling order
