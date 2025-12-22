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

#ifndef CLOCK_HPP
#define CLOCK_HPP

#include <Scheduler.hpp>

/*
Clock provides a squarewave signal with controllable high and low times.
It can also be disabled by calling Clock.disable() and Clock.restart().
Example:

// Blinky
MainSchedule schedule;
const int ledPin = 17;
long lowTime = 100;
long highTime = 200;
long signal;
Clock clk(schedule, lowTime, highTime, signal);
DigitalLED led(schedule, signal, ledPin);
void setup() { schedule.begin(); }
void loop() { schedule.poll(); }
*/

class Expires {
public:
	virtual bool expired() const = 0;
	virtual void reset(long time) = 0;
};

/* Pro tip: you can 'turn off' a timer by setting the delay to MAX_LONG. */
class Timer : public Expires {
	long _time;
	long _lastExpired;
public:
	Timer(long time = 0) : _time(constrain(time, 0, MAX_LONG)) {
		_lastExpired = millis();
	}
	bool expired() const {
		return millis() - _lastExpired > _time;
	}
	void reset() {
		reset(_time);
	}
	void reset(long time) {
		_time = constrain(time, 0, MAX_LONG);
		_lastExpired = millis();
	}
};

class ExpiresComposite : public Composite<Expires> {
	const bool _any; // true for any(expired), false for all(expired
public:
	ExpiresComposite(bool any = true, Expires *itemsZ[] = NULL) :
		Composite<Expires>(itemsZ, countZ(itemsZ)), _any(any) { }
	bool expired() const {
		for (int i = 0; i < length(); i++) {
			if (item(i)->expired() && _any) {
				return true;
			}
			if (!item(i)->expired() && !_any) {
				return false;
			}
		}
		return !_any;
	}
};

class PeriodicBase : private Scheduled, public Enabled, public Timer {
	bool _enabled = true;
	long &_period;
public:
	PeriodicBase(Schedule &schedule, long &period) : Scheduled(schedule), Timer(period), _period(period) { }
	void poll() {
		if (expired() && _enabled) {
			reset(_period);
			handleExpired();
		}
	}
	void enable(bool value) { _enabled = value; }
	void toggle() { enable(!_enabled); }
	bool enabled() const { return _enabled; }
	virtual void handleExpired() = 0;
};

class Clock : private Scheduled, private Timer, public Enabled {
	long &_lowTime;
	long &_highTime;
	bool &_value;
	bool _enabled = true;
public:
	Clock(Schedule &schedule, long &lowTime, long &highTime, bool &value) :
		Scheduled(schedule), Timer(lowTime), _lowTime(lowTime), _highTime(highTime), _value(value) { }
	void enable(bool value) {
		if (_enabled != value) {
			_enabled = value;
			_value = LOW;
			reset(0);
		}
	}
	void toggle() {
		enable(!_enabled);
	}
	bool enabled() const {
		return _enabled;
	}
	void poll() {
		if (expired() && _enabled) {
			if (_value) {
				_value = LOW;
				reset(_lowTime);
			} else {
				_value = HIGH;
				reset(_highTime);
			}
		}
	}
};

class SpeedTest : public Scheduled {
	Timer _oneSecond;
	uint16_t _count;
public:
	SpeedTest(Schedule &schedule) : Scheduled(schedule), _count(0), _oneSecond(1000) { }
	void poll() {
		_count++;
		if (_oneSecond.expired()) {
			_oneSecond.reset(1000);
			Serial.print("PollsPerSecond:");
			Serial.println(_count, DEC);
			_count = 0;
		}
	}
};

#endif