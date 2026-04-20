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

#ifndef EDGEDETECTOR_HPP
#define EDGEDETECTOR_HPP

#include <Scheduler.hpp>
#include <Clock.hpp>

class EdgeDetectorBase : private Scheduled {
	bool &_value;
	bool _last;
public:
	EdgeDetectorBase(Schedule &schedule, bool &value) :
		Scheduled(schedule), _value(value), _last(value) { }
	void poll() {
		bool current = _value;
		if (_last != current) {
			_last = current;
			if (current) {
				onRisingEdge();
			} else {
				onFallingEdge();
			}
		}
	}
	virtual void onRisingEdge() = 0;
	virtual void onFallingEdge() = 0;
};

class Trigger {
public:
	virtual void fire() = 0;
};

class TriggerFunction : public Trigger {
	void (*_handler)();
public:
	TriggerFunction(void (*handler)() = 0) : _handler(handler) { }
	TriggerFunction(const TriggerFunction &trigger) : _handler(trigger._handler) { }
	void fire() { if (_handler) _handler(); }
};

class EdgeDetector : private EdgeDetectorBase {
	Trigger &_risingTrigger;
	Trigger &_fallingTrigger;
	TriggerFunction _risingWrapper;
	TriggerFunction _fallingWrapper;
public:
	EdgeDetector(Schedule &schedule, bool &value, Trigger &risingTrigger, Trigger &fallingTrigger) :
		EdgeDetectorBase(schedule, value),
		_risingTrigger(risingTrigger), _fallingTrigger(fallingTrigger),
		_risingWrapper(0), _fallingWrapper(0) { }
	EdgeDetector(Schedule &schedule, bool &value, void (*risingHandler)(), void (*fallingHandler)() = 0) :
		EdgeDetectorBase(schedule, value),
		_risingTrigger(_risingWrapper), _fallingTrigger(_fallingWrapper),
		_risingWrapper(risingHandler), _fallingWrapper(fallingHandler) { }
	void onRisingEdge() { _risingTrigger.fire(); }
	void onFallingEdge() { _fallingTrigger.fire(); }
};

class PeriodicTrigger : public PeriodicBase {
	Trigger &_trigger;
public:
	PeriodicTrigger(Schedule &schedule, long &period, Trigger &trigger) :
		PeriodicBase(schedule, period), _trigger(trigger) { }
	void handleExpired() { _trigger.fire(); }
};

/* This won't actually work because the events may come faster than the delay time.
class DelayValueTrigger : public Trigger, private Scheduled {
	Timer _timer;
	const long _delay; // would I want to let this float?
	const bool _value;
	bool &_output;
public:
	DelayValueTrigger(Schedule &schedule, bool value, long delay, bool &output) :
		Scheduled(schedule),
		_timer(MAX_LONG), _delay(delay), _value(value), _output(output) { }
	void fire() { _timer.reset(_delay); }
	void poll() {
		if (_timer.expired()) {
			_output = _value;
			_timer.reset(MAX_LONG);
		}
	}
};

class EdgeFollower : private EdgeDetector {
	DelayValueTrigger _risingTrigger;
	DelayValueTrigger _fallingTrigger;
public:
	EdgeFollower(Schedule &schedule, bool &input, long delayValue, bool &output) :
		EdgeDetector(schedule, input, _risingTrigger, _fallingTrigger),
		_risingTrigger(schedule, HIGH, delayValue, output),
		_fallingTrigger(schedule, LOW, delayValue, output) { }
};
*/

class Counter : private EdgeDetectorBase {
	long &_output;
public:
	Counter(Schedule &schedule, bool &input, long &output) :
		EdgeDetectorBase(schedule, input), _output(output) {
			reset();
		}
	void reset() { _output = 0; }
	void onRisingEdge() { _output++; }
	void onFallingEdge() { }
};

class FrequencyDivider : private Scheduled {
	Counter _counter;
	bool &_input;
	long _count;
	const long _divisor;
	bool &_output;
public:
	FrequencyDivider(Schedule &schedule, bool &input, long divisor, bool &output) :
		_counter(schedule, input, _count), Scheduled(schedule),
		_input(input), _output(output), _divisor(divisor) { }
	void poll() {
		if (_count == _divisor) {
			_output = !_output;
			_counter.reset();
		}
	}
	void reset() { _counter.reset(); }
};

#endif