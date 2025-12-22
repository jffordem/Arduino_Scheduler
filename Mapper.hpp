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

#ifndef MAPPER_HPP
#define MAPPER_HPP

#include <Scheduler.hpp>

/*
Mapper wraps the map() function in a composable object. See EncoderWheel.h for how it's used.
Inverter is a controllable inverter. See DigitalLED for how it's used.
Constrain holds a value to a certain range, but I haven't actually found a use for it yet.
*/

template <class Tin, class Tout>
class Mapper : public Scheduled {
	Tin &_inValue;
	Tout &_outValue;
	const Tin _inLow;
	const Tin _inHigh;
	const Tout _outLow;
	const Tout _outHigh;
public:
	Mapper(Schedule &schedule, Tin &inValue, Tout &outValue, Tin inLow, Tin inHigh, Tout outLow, Tout outHigh) : 
		Scheduled(schedule), _inValue(inValue), _outValue(outValue), _inLow(inLow), _inHigh(inHigh), _outLow(outLow), _outHigh(outHigh) { }
	void poll() {
		_outValue = map(_inValue, _inLow, _inHigh, _outLow, _outHigh);
	}
};

template <class Tin, class Tout>
class Chooser : public Scheduled {
	Tin &_inValue;
	Tout &_outValue;
	Tout *_optionsZ;
	long _optionsCount;
public:
	Chooser(Schedule &schedule, Tin &inValue, Tout &outValue, Tout *optionsZ) : 
		Scheduled(schedule), _inValue(inValue), _outValue(outValue), _optionsZ(optionsZ) { _optionsCount = countZ(optionsZ); }
	void poll() {
		long index = (long) _inValue;
		while (index < 0) index += _optionsCount;
		while (index >= _optionsCount) index -= _optionsCount;
		if (index < 0 || index >= _optionsCount) {
			_outValue = _optionsZ[0];
		} else {
			_outValue = _optionsZ[index];
		}
	}
};

class Inverter : public Scheduled {
	bool &_input;
	bool &_output;
	const bool _invert;
public:
	Inverter(Schedule &schedule, bool &input, bool &output, bool invert = true) :
		Scheduled(schedule), _input(input), _output(output), _invert(invert) { }
	void poll() {
		// During initialization _input can have a garbage value
		int temp = (int) _input;
		if (temp == HIGH || temp == LOW) {
			_output = (_input ^ _invert);
		}
	}
};

class AndInputs : private Scheduled {
	bool &_a;
	bool &_b;
	bool &_x;
public:
	AndInputs(Schedule &schedule, bool &a, bool &b, bool &x) :
		Scheduled(schedule), _a(a), _b(b), _x(x) { }
	void poll() { _x = _a && _b; }
};

class OrInputs : private Scheduled {
	bool &_a;
	bool &_b;
	bool &_x;
public:
	OrInputs(Schedule &schedule, bool &a, bool &b, bool &x) :
		Scheduled(schedule), _a(a), _b(b), _x(x) { }
	void poll() { _x = _a || _b; }
};

template <class T>
class Constrain : public Scheduled {
	const T _min;
	const T _max;
	T &_input;
	T &_output;
public:
	Constrain(Schedule &schedule, T &input, T &output, T minVal, T maxVal) :
		Scheduled(schedule), _input(input), _output(output), _min(min(minVal, maxVal)), _max(max(minVal, maxVal)) { }
	void poll() {
		_output = constrain(_input, _min, _max);
	}
};

#endif
