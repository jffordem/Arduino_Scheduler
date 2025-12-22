/*
MIT License

Copyright (c) 2022 jffordem

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

#ifndef HID_HPP
#define HID_HPP

#include <Scheduler.hpp>
#include <Clock.hpp>
#include <EdgeDetector.hpp>
#include <Mapper.hpp>

/*
These objects are for controlling keyboard and mouse buttons.  (I haven't bothered with mouse movement yet.)

Examples:
MainSchedule schedule;
long castTime = 700;
long waitTime = 1800;
MouseButton leftMouse(MOUSE_LEFT);
ButtonController castSpell(schedule, castTime, waitTime, leftMouse);
void setup() {
	Mouse.begin();
	Keyboard.begin();
	schedule.begin();
	delay(8000);
}
void loop() {
	schedule.poll();
}
*/

/**
 * ValuePresser watches the given value and calls press when it goes HIGH and
 * release when it falls LOW.
*/
class ValuePresser : private EdgeDetectorBase {
	Pressable &_button;
public:
	ValuePresser(Schedule &schedule, bool &value, Pressable &button) : 
		EdgeDetectorBase(schedule, value), _button(button) { }
	void onRisingEdge() { _button.press(); }
	void onFallingEdge() { _button.release(); }
};

/**
 * PressHandler translates press() and release() calls into external functions
 * for customized handling.
*/
class PressHandler : public Pressable {
	void (*_pressHandler)();
	void (*_releaseHandler)();
public:
	PressHandler(void (*pressHandler)(), void (*releaseHandler)()) : _pressHandler(pressHandler), _releaseHandler(releaseHandler) { }
	void press() {
		if (_pressHandler) {
			_pressHandler();
		}
	}
	void release() {
		if (_releaseHandler) {
			_releaseHandler();
		}
	}
};

/**
 * Pressable for a keyboard key, like quicksave (KEY_F5).
*/
class KeyPress : public Pressable {
	int _key;
public:
	KeyPress(int key) : _key(key) { }
	void press() { Keyboard.press(_key); }
	void release() { Keyboard.release(_key); }
	void assign(int key) { Keyboard.release(_key); _key = key; }
};

class KeyPressDynamic : public Pressable {
	char &_key;
	char _pressed;
public:
	KeyPressDynamic(char &key) : _key(key), _pressed(0) { }
	void press() { Keyboard.press(_key); _pressed = _key; }
	void release() { if (_pressed) Keyboard.release(_pressed); _pressed = 0; }
};

/**
 * Pressable for a mouse button like MOUSE_LEFT or MOUSE_RIGHT.
*/
class MouseButton : public Pressable {
	int _button;
public:
	MouseButton(int button) : _button(button) { }
	void press() { Mouse.press(_button); }
	void release() { Mouse.release(_button); }
	void assign(int button) { Mouse.release(_button); _button = button; }
};

/**
 * DummyButton is for troubleshooting so that the keyboard and
 * mouse don't start doing weird things while you're debugging.
*/
class DummyButton : public Pressable {
	const bool _verbose;
	const char *_name;
public:
	DummyButton(const char *name = "BUTTON", bool verbose = false) : _name(name), _verbose(verbose) { }
	void press() { if (_verbose) { printMillis(); Serial.print(_name); Serial.println(" PRESS"); } }
	void release() { if (_verbose) { printMillis(); Serial.print(_name); Serial.println(" RELEASE"); } }
private:
	void printMillis() { Serial.print("["); Serial.print(millis(), DEC); Serial.print("] "); }
};

/**
 * ButtonController creates a simple clock to drive a mouse or keyboard button.
*/
class ButtonController : public Clock, private ValuePresser {
	bool _value = LOW;
public:
	ButtonController(Schedule &schedule, long &releaseTime, long &pressTime, Pressable &button) :
		Clock(schedule, releaseTime, pressTime, _value),
		ValuePresser(schedule, _value, button) { }
};

/* It's assumed that the delay is much shorter than the press/release times. */
class PressFollower : private Scheduled, public Pressable {
	Timer _pressTimer;
	Timer _releaseTimer;
	const long _delay;
	Pressable &_output;
public:
	PressFollower(Schedule &schedule, long delayValue, Pressable &output) :
		Scheduled(schedule),
		_pressTimer(MAX_LONG), _releaseTimer(MAX_LONG),
		_delay(delayValue), _output(output) { }
	void poll() {
		if (_pressTimer.expired()) {
			_output.press();
			_pressTimer.reset(MAX_LONG);
		}
		if (_releaseTimer.expired()) {
			_output.release();
			_releaseTimer.reset(MAX_LONG);
		}
	}
	void press() { _pressTimer.reset(_delay); }
	void release() { _releaseTimer.reset(_delay); }
};

#endif
