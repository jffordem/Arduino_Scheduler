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

#ifndef ENCODERWHEEL_HPP
#define ENCODERWHEEL_HPP

#include <Scheduler.hpp>
#include <PinIO.hpp>
#include <EdgeDetector.hpp>
#include <Mapper.hpp>
#include <SerialPlot.hpp>

/*
EncoderWheel and EncoderControl allow the encoder wheel to be an input to
a value and control it along its range, starting at the midpoint.

Example:
const int clkPin = 5;
const int dtPin = 6;
MainSchedule schedule;
long value = 250;
EncoderControl encoder(schedule, clkPin, dtPin, value, 20, value*2);
void setup() {
	Serial.begin(9600);
	schedule.begin();
}
void loop() {
	schedule.poll();
	Serial.print("VALUE: ");
	Serial.println(value);
}
*/

struct EncoderConfig {
	int clockPin;
	int dataPin;
	EncoderConfig(int clkPinValue, int dtPinValue) : clockPin(clkPinValue), dataPin(dtPinValue) { }
};

class EncoderWheelHandler : public EdgeDetectorBase {
	DigitalRead _clk;
	DigitalRead _data;
	bool _clkValue;
	bool _dtValue;
public:
	static const uint8_t ENCODER_NONE = 0;
	static const uint8_t ENCODER_WHEEL_LEFT = 1;
	static const uint8_t ENCODER_WHEEL_RIGHT = 2;

	virtual void handleInput(uint8_t input) = 0;

	EncoderWheelHandler(Schedule &schedule, const EncoderConfig &config) :
		EncoderWheelHandler(schedule, config.clockPin, config.dataPin) { }
	EncoderWheelHandler(Schedule &schedule, int clockPin, int dataPin) : 
		EdgeDetectorBase(schedule, _clkValue),
		_clk(schedule, clockPin, _clkValue, INPUT_PULLUP), 
		_data(schedule, dataPin, _dtValue, INPUT_PULLUP) { }
	void onRisingEdge() {
		if (_dtValue != _clkValue) {
			handleInput(ENCODER_WHEEL_RIGHT);
		} else {
			handleInput(ENCODER_WHEEL_LEFT);
		}
	}
	void onFallingEdge() { }
	void plot(PlotComposite &plot, String name) {
		PlotBool::addToPlot(plot, name + ".clock", _clkValue);
		PlotBool::addToPlot(plot, name + ".data", _dtValue);
	}
};

class EncoderWheel : public EncoderWheelHandler {
	int &_value;
	int _limit;
public:
	EncoderWheel(Schedule &schedule, const EncoderConfig &config, int &value, int limit = (MAX_INT - 10)) :
		EncoderWheel(schedule, config.clockPin, config.dataPin, value, limit) { }
	EncoderWheel(Schedule &schedule, int clockPin, int dataPin, int &value, int limit = (MAX_INT - 10)) : 
		EncoderWheelHandler(schedule, clockPin, dataPin), _value(value), _limit(limit) { }
	void handleInput(uint8_t input) {
		if (input == ENCODER_WHEEL_LEFT) {
			_value = constrain(_value - 1, 0, _limit);
		} else if (input == ENCODER_WHEEL_RIGHT) {
			_value = constrain(_value + 1, 0, _limit);
		}
	}
	void plot(PlotComposite &plot, String name) {
		PlotNum<int>::addToPlot(plot, name + ".value", _value);
	}
};

template <class T>
class EncoderControl : private EncoderWheel, private Mapper<int, T> {
	int _encoderValue = 0;
public:
	EncoderControl(Schedule &schedule, const EncoderConfig &config, T &value, int sensitivity, T maxVal) :
		EncoderControl(schedule, config.clockPin, config.dataPin, value, sensitivity, maxVal) { }
	EncoderControl(Schedule &schedule, int clockPin, int dataPin, T &value, int sensitivity, T maxVal) :
    	EncoderWheel(schedule, clockPin, dataPin, _encoderValue, abs(sensitivity)),
    	Mapper<int, T>(schedule, _encoderValue, value, -sensitivity, sensitivity, 0, maxVal) { }
	void plot(PlotComposite &plot, String name) {
		EncoderWheel::plot(plot, name);
		// PlotNum<int>::addToPlot(plot, name, ".value", _encoderValue);
	}
};

template <class T>
class EncoderSelector : private EncoderWheel, private Chooser<int, T> {
	int _encoderValue = 0;
public:
	EncoderSelector(Schedule &schedule, const EncoderConfig &config, T &value, T optionsZ[]) :
		EncoderSelector(schedule, config.clockPin, config.dataPin, value, optionsZ) { }
	EncoderSelector(Schedule &schedule, int clockPin, int dataPin, T &value, T optionsZ[]) :
		EncoderWheel(schedule, clockPin, dataPin, _encoderValue),
		Chooser<int, T>(schedule, _encoderValue, value, optionsZ),
		_encoderValue(findIndex(value, optionsZ)) { }
	void plot(PlotComposite &plot, String name) {
		EncoderWheel::plot(plot, name);
		// PlotNum<int>::addToPlot(plot, name, ".selection", _encoderValue);
	}
private:
	static int findIndex(T value, T optionsZ[]) {
		for (int i = 0; optionsZ[i]; i++) {
			if (optionsZ[i] == value) {
				return i;
			}
		}
		return 0;
	}
};

#endif
