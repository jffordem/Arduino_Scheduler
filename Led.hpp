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

#ifndef LED_HPP
#define LED_HPP

#include <Scheduler.hpp>
#include <PinIO.hpp>
#include <Mapper.hpp>

/*
These classes control LEDs.  See Clock.h for an example.
*/

struct LedConfig {
	int pin;
	bool lowIsOn;
	LedConfig(int pinValue, bool lowIsOnValue) : pin(pinValue), lowIsOn(lowIsOnValue) { }
};

/**
 * DigitalLED turns the LED on when the given value is HIGH
 * and off when the value is LOW.  This normalizes different
 * hardware where some LEDs are pulled LOW to go on.
*/
class DigitalLED : private Inverter, private DigitalWrite {
	bool _on;
public:
	DigitalLED(Schedule &schedule, bool &value, const LedConfig &config) :
		DigitalLED(schedule, value, config.pin, config.lowIsOn) { }
	DigitalLED(Schedule &schedule, bool &value, int pin, bool lowIsOn = false) : 
		Inverter(schedule, value, _on, lowIsOn),
		DigitalWrite(schedule, _on, pin) { }
};

// Pins a - g are top, ur, lr, bot, ll, ul, mid
class SevenSegLED : public Scheduled {
	static const int num_pins = 7;
	const int *_pins;
	const bool _lowIsOn;
	short &_value;
public:
	SevenSegLED(Schedule &schedule, int *pins, short &value, bool lowIsOn = true) :
		Scheduled(schedule), _pins(pins), _lowIsOn(lowIsOn), _value(value) {
		for (int i = 0; i < num_pins; i++) {
			pinMode(_pins[i], OUTPUT);
		}
	}
	void poll() {
		int current = _value;
		static const int digits[10][num_pins] = {
			{1, 1, 1, 1, 1, 1, 0}, // 0
			{0, 1, 1, 0, 0, 0, 0}, // 1
			{1, 1, 0, 1, 1, 0, 1}, // 2
			{1, 1, 1, 1, 0, 0, 1}, // 3
			{0, 1, 1, 0, 0, 1, 1}, // 4
			{1, 0, 1, 1, 0, 1, 1}, // 5
			{0, 0, 1, 1, 1, 1, 1}, // 6
			{1, 1, 1, 0, 0, 0, 0}, // 7
			{1, 1, 1, 1, 1, 1, 1}, // 8
			{1, 1, 1, 0, 0, 1, 1}  // 9
		};
		for (int i = 0; i < num_pins; i++) {
			digitalWrite(_pins[i], getPinValue(digits[current][i]));
		}
	}
private:
	int getPinValue(int on) { return ((on == 1) ^ _lowIsOn) ? HIGH : LOW; }
};

/* Not sure where this should go.  This is an example of OO-based composition of these types. */
template <class Tin, class Tout>
class Pot : private AnalogRead<Tin>, private Mapper<Tin, Tout> {
	Tin _rawValue;
public:
	Pot(Schedule &schedule, int pin, Tout &value, Tout minVal, Tout maxVal) :
		AnalogRead<Tin>(schedule, pin, _rawValue),
		Mapper<Tin, Tout>(schedule, _rawValue, value, minVal, maxVal) { }
};

#endif
