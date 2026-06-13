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

#pragma once

#include <Scheduler.hpp>

/*
Wrappers for reading/writing to pins.  (No support for interrupts yet.)
See DigitalLED for an example.
*/

class DigitalRead : public Scheduled {
	bool &_value;
	const int _pin;
public:
	DigitalRead(Schedule &schedule, int pin, bool &value, int mode = INPUT_PULLUP) : Scheduled(schedule), _pin(pin), _value(value) {
		pinMode(pin, mode);
	}
	void poll() {
		_value = digitalRead(_pin);
	}
};

class DigitalWrite : public Scheduled {
	bool &_value;
	const int _pin;
public:
	DigitalWrite(Schedule &schedule, bool &value, int pin) : Scheduled(schedule), _pin(pin), _value(value) {
		pinMode(pin, OUTPUT);
	}
	void poll() {
		digitalWrite(_pin, _value);
	}
};

template <class T>
class AnalogRead : public Scheduled {
	T &_value;
	const int _pin;
public:
	AnalogRead(Schedule &schedule, int pin, T &value) : Scheduled(schedule), _pin(pin), _value(value) {
		pinMode(pin, INPUT);
	}
	void poll() {
		_value = analogRead(_pin);
	}
};

template <class T>
class AnalogWrite : public Scheduled {
	const int _pin;
	T &_value;
public:
	AnalogWrite(Schedule &schedule, T &value, int pin) : Scheduled(schedule), _pin(pin), _value(value) {
		pinMode(pin, OUTPUT);
	}
	void poll() {
		analogWrite(_pin, _value);
	}
};
