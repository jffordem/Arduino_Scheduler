# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

A header-only C++ library for Arduino development. It provides composable abstractions for inputs (buttons, encoders, keypads), outputs (LEDs, HID keyboard/mouse), display/UI (character LCDs, menus), and a central polling scheduler that coordinates them. The original use case was controlling repeated casting in Skyrim via an encoder wheel.

## Build & Test

No standalone build system — this compiles as standard Arduino sketches. Include only the headers you need; the library is header-only so unused modules add no cost. Examples in `examples/` cover each major module.

Enable the `DEBUG` macro in `Arduino.hpp` to activate serial debug output.

## Core Architecture

### The Scheduler Pattern

Everything revolves around `MainSchedule`. Objects that need periodic polling inherit from `Scheduled` and pass `Schedule &schedule` as their **first constructor parameter** — this auto-registers them. The sketch calls `schedule.begin()` in `setup()` and `schedule.poll()` in `loop()`. Max 50 pollers (configurable via `MAX_POLLERS` in `Scheduler.hpp`).

Polling is sequential and deterministic — no interrupts. All input detection happens by polling.

### Composition over Inheritance

Complex units are built by combining simpler ones:
- Use `public` inheritance for interfaces/composites
- Use `private` inheritance to hide implementation details
- Pass the same `schedule` reference through the composition chain

The `Blinky` example in the README shows the idiom: `class Blinky : public Clock, private DigitalLED`.

### Key Interfaces (Scheduler.hpp)

| Interface | Method(s) | Meaning |
|-----------|-----------|---------|
| `Poller` | `poll()` | Gets called every cycle |
| `Pressable` | `press()` / `release()` | Button-like |
| `Enabled` | `enable()` / `toggle()` / `enabled()` | On/off |
| `Trigger` | `fire()` | One-shot event |

Composite variants (`PressComposite`, `EnableComposite`, `PollerComposite`) broadcast to all registered members.

### Hardware Normalization

Config structs use boolean flags to abstract hardware differences:
- `ButtonConfig`: `lowIsPressed` — handles active-high and active-low buttons
- `LedConfig`: `lowIsOn` — handles common-cathode and common-anode LEDs
- `EncoderConfig`: `clockPin` / `dataPin`

### Dependency Tree

```
Arduino.hpp (core shim)
├── LinkedList.hpp
└── Scheduler.hpp
    ├── Clock.hpp
    ├── PinIO.hpp
    ├── EdgeDetector.hpp
    ├── HIDIO.hpp
    ├── Mapper.hpp
    ├── SerialPlot.hpp
    ├── Led.hpp → PinIO + Mapper
    ├── ButtonHandler.hpp → EdgeDetector + PinIO + Mapper
    ├── EncoderWheel.hpp → PinIO + EdgeDetector + Mapper + SerialPlot
    ├── Graphics.hpp → Clock + EdgeDetector
    ├── Display.hpp → Scheduler
    ├── MenuUI.hpp → Display + Scheduler
    ├── KeypadHandler.hpp → Scheduler
    └── LK204_25.hpp → Wire/I2C LCD + Keypad driver
```

## Module Quick Reference

**Start here for new sketches:** `Scheduler.hpp`, `Clock.hpp`, `Led.hpp`

| Goal | Headers |
|------|---------|
| Button input | `ButtonHandler.hpp`, `EdgeDetector.hpp` |
| Encoder input | `EncoderWheel.hpp`, `Mapper.hpp` |
| Keyboard/Mouse HID | `HIDIO.hpp` |
| Character LCD + menu | `Display.hpp`, `MenuUI.hpp`, `LK204_25.hpp` |
| Value transforms / logic | `Mapper.hpp` |
| Real-time debug | `SerialPlot.hpp` |
| Known hardware layouts | `BreadboardConfig.hpp`, `LeonardoConfig.hpp` |

## Display & Menu System (Recent Additions)

`Display.hpp` provides `DisplayBuffer<TRows, TCols>` — a character buffer with clear/set/write, plus `DisplayDrawable` for retained-mode rendering.

`MenuUI.hpp` builds a menu system on top of it. `MenuItem` is created via factory methods and supports these kinds: `Action`, `Submenu`, `ToggleEnabled`, `EditLong`, `EnterLong`, `EnterString`. `MenuContext` tracks navigation state. See `examples/HiLoGame/` for a complete working example with the LK204-25 LCD and keypad.

## Guidelines

- **Poll cost matters**: avoid heavy computation inside `poll()` — it runs every cycle for every registered poller.
- **No dynamic allocation**: pre-allocate; respect `MAX_POLLERS`.
- **New pollers**: always accept `Schedule &schedule` as first parameter so they auto-register.
- **Value transforms**: reach for `Mapper`, `Constrain`, `Inverter`, `AndInputs`/`OrInputs` before writing custom logic.
