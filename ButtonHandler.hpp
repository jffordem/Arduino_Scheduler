/*
MIT License

Copyright (c) 2022-2025 jffordem

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef BUTTONHANDLER_HPP
#define BUTTONHANDLER_HPP

#include <Scheduler.hpp>
#include <EdgeDetector.hpp>
#include <PinIO.hpp>
#include <Mapper.hpp>

/*
Button and ButtonHandler monitors a pushbutton (or switch) and provides
handling for the rising edge and/or falling edge.  Examples:

MainSchedule schedule;
const int btnPin = 3;
ButtonHandler btn(schedule, btnPin, &onPress, 0);
void onPress() { Serial.println("PRESSED!");}
void setup() {
  Serial.begin();
  schedule.begin();
}
void loop() { schedule.poll(); }
*/

struct ButtonConfig {
	int pin;
	bool lowIsPressed;
	ButtonConfig(int pinValue, bool lowIsPressedValue) :
		pin(pinValue), lowIsPressed(lowIsPressedValue) { }
};

// Suppresses output changes until the input has been stable for debounceMs.
// Eliminates contact bounce on mechanical buttons without blocking poll().
class DebounceFilter : private Scheduled {
	const bool &_input;
	bool &_output;
	bool _candidate;
	long _debounceMs;
	Timer _timer;
public:
	DebounceFilter(Schedule &schedule, const bool &input, bool &output, long debounceMs = 10) :
		Scheduled(schedule), _input(input), _output(output),
		_candidate(false), _debounceMs(debounceMs), _timer(0) { }
	void poll() override {
		if (_input != _candidate) {
			_candidate = _input;
			_timer.reset(_debounceMs);
		} else if (_timer.expired()) {
			_output = _candidate;
		}
	}
};

/**
 * ButtonValue reads a pushbutton, normalizes the signal so value is HIGH when
 * pressed regardless of hardware wiring, then debounces it.
 */
class ButtonValue : private DigitalRead, private Inverter, private DebounceFilter {
	bool _rawValue;
	bool _normalizedValue;
public:
	ButtonValue(Schedule &schedule, const ButtonConfig &config, bool &value) :
		ButtonValue(schedule, config.pin, config.lowIsPressed, value) { }
	ButtonValue(Schedule &schedule, int pin, bool pulledLowOnPress, bool &value) :
		DigitalRead(schedule, pin, _rawValue, pinMode(pulledLowOnPress)),
		Inverter(schedule, _rawValue, _normalizedValue, pulledLowOnPress),
		DebounceFilter(schedule, _normalizedValue, value),
		_rawValue(pulledLowOnPress), _normalizedValue(false) { }
private:
	static int pinMode(bool pulledLowOnPress) {
		if (pulledLowOnPress) return INPUT_PULLUP;
		return INPUT;
	}
};

/**
 * ButtonHandler lets you pass in a pressHandler, and an optional releaseHandler function
 * to customize behavior when buttons are pushed.
 */
class ButtonHandler : private ButtonValue, private EdgeDetector {
	bool _value;
public:
	ButtonHandler(Schedule &schedule, const ButtonConfig &config, void (*pressHandler)(), void (*releaseHandler)() = 0) :
		ButtonHandler(schedule, config.pin, config.lowIsPressed, pressHandler, releaseHandler) { }
	ButtonHandler(Schedule &schedule, int pin, bool pulledLowOnPress, void (*pressHandler)(), void (*releaseHandler)() = 0) :
		ButtonValue(schedule, pin, pulledLowOnPress, _value),
		EdgeDetector(schedule, _value, pressHandler, releaseHandler),
		_value(LOW) { }
};

/**
 * Button transfers the pressing of a physical button pressing to a Pressable object's press/release methods.
 */
class Button : private EdgeDetectorBase, private ButtonValue {
	Pressable &_button;
	bool _value;
public:
	Button(Schedule &schedule, const ButtonConfig &config, Pressable &button) :
		Button(schedule, config.pin, config.lowIsPressed, button) { }
	Button(Schedule &schedule, int pin, bool pulledLowOnPress, Pressable &button) :
		ButtonValue(schedule, pin, pulledLowOnPress, _value),
		EdgeDetectorBase(schedule, _value), _button(button), _value(LOW) { }
	void onRisingEdge() {
		_button.press();
	}
	void onFallingEdge() {
		_button.release();
	}
};

// Turning a clock on/off seemed to be a common thing to do, so here's a composite.
class ToggleButton : private Button, private Pressable {
	Enabled &_control;
public:
	ToggleButton(Schedule &schedule, const ButtonConfig &config, Enabled &control) :
		ToggleButton(schedule, config.pin, config.lowIsPressed, control) { }
	ToggleButton(Schedule &schedule, int pin, bool pulledLowOnPress, Enabled &control) :
		Button(schedule, pin, pulledLowOnPress, *this), _control(control) { }
	void press() { _control.toggle(); }
	void release() { }
};

/* This doesn't seem to require a PWM pin. */
class ActiveBuzzer : private DigitalWrite {
public:
	ActiveBuzzer(Schedule &schedule, bool &value, int pin) :
		DigitalWrite(schedule, value, pin) { }
};

/* Not clear if this requires PWM pin, or regular one. */
class PassiveBuzzer : private EdgeDetectorBase {
	uint16_t _frequency;
	uint8_t _pin;
public:
	PassiveBuzzer(Schedule &schedule, bool &value, uint8_t pin, uint16_t frequency) :
		EdgeDetectorBase(schedule, value), _pin(pin), _frequency(frequency) { }
	void onRisingEdge() { tone(_pin, _frequency); }
	void onFallingEdge() { noTone(_pin); }
};

#endif
