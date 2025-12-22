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

#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include <Scheduler.hpp>
#include <Clock.hpp>
#include <string.h>
#include <stdio.h>

template <int TRows, int TCols>
class DisplayBuffer {
	char _data[TRows][TCols];
public:
	static const int Rows = TRows;
	static const int Cols = TCols;
	DisplayBuffer(char fill = ' ') { clear(fill); }
	void clear(char fill = ' ') {
		for (int r = 0; r < TRows; r++) {
			for (int c = 0; c < TCols; c++) {
				_data[r][c] = fill;
			}
		}
	}
	char get(int row, int col) const { return _data[row][col]; }
	void set(int row, int col, char ch) {
		if (row < 0 || row >= TRows || col < 0 || col >= TCols) return;
		_data[row][col] = ch;
	}
	void write(int row, int col, const char *text, int width = -1) {
		if (!text) text = "";
		if (width < 0) width = (int)strlen(text);
		for (int i = 0; i < width; i++) {
			char ch = text[i];
			if (ch == 0) ch = ' ';
			set(row, col + i, ch);
		}
	}
};

template <class TDisplay, int TRows = 4, int TCols = 20>
class DisplayDrawable {
public:
	virtual void draw(DisplayBuffer<TRows, TCols> &buffer) = 0;
};

template <class TDisplay, int TRows = 4, int TCols = 20>
class MainDisplay : private Scheduled {
    TDisplay &_display;
    DisplayDrawable<TDisplay, TRows, TCols> &_drawable;
 	Timer _tick;
 	Timer _full;
 	DisplayBuffer<TRows, TCols> _desired;
 	DisplayBuffer<TRows, TCols> _flushed;
 	bool _hasFlushed;
public:
    MainDisplay(Schedule &schedule, TDisplay &display, DisplayDrawable<TDisplay, TRows, TCols> &drawable, long period, long fullRefreshPeriod = 5000L) : 
        Scheduled(schedule),
 		_display(display),
 		_drawable(drawable),
 		_tick(period),
 		_full(fullRefreshPeriod),
 		_desired(' '),
 		_flushed(' '),
 		_hasFlushed(false) { }
    void begin() {
 		_tick.reset();
 		_full.reset();
        _display.begin();
        _display.backlight();
        _display.clear();
        _display.home();
        _display.noCursor();
		_hasFlushed = false;
    }
    void poll() override {
		if (_tick.expired()) {
			_tick.reset();
			bool forceFull = false;
			if (_full.expired()) {
				_full.reset();
				forceFull = true;
			}
			render();
			flush(forceFull);
		}
    }
 private:
 	void render() {
 		_desired.clear(' ');
 		_drawable.draw(_desired);
 	}
 	void flush(bool forceFull) {
 		if (forceFull || !_hasFlushed) {
 			_flushed.clear((char)0);
 		}
 		for (int row = 0; row < DisplayBuffer<TRows, TCols>::Rows; row++) {
 			int col = 0;
 			while (col < DisplayBuffer<TRows, TCols>::Cols) {
 				char desiredCh = _desired.get(row, col);
 				char flushedCh = _flushed.get(row, col);
 				bool changed = forceFull || !_hasFlushed || desiredCh != flushedCh;
 				if (!changed) {
 					col++;
 					continue;
 				}
 				int start = col;
 				char run[DisplayBuffer<TRows, TCols>::Cols + 1];
 				int runLen = 0;
 				while (col < DisplayBuffer<TRows, TCols>::Cols) {
 					desiredCh = _desired.get(row, col);
 					flushedCh = _flushed.get(row, col);
 					changed = forceFull || !_hasFlushed || desiredCh != flushedCh;
 					if (!changed) break;
 					run[runLen++] = desiredCh;
 					_flushed.set(row, col, desiredCh);
 					col++;
 				}
 				run[runLen] = 0;
 				_display.setCursor(start, row);
 				_display.print(run);
 			}
 		}
 		_hasFlushed = true;
 	}
};

template <class TDisplay, int TRows = 4, int TCols = 20>
class DisplayLabel : public DisplayDrawable<TDisplay, TRows, TCols> {
    int _row;
    int _col;
    const char *_text;
public:
    DisplayLabel(int row, int col, const char *text) : _row(row), _col(col), _text(text) { }
    void draw(DisplayBuffer<TRows, TCols> &buffer) {
		buffer.write(_row, _col, _text);
    }
};

template <class TDisplay, int TRows = 4, int TCols = 20>
class Spinner : public DisplayDrawable<TDisplay, TRows, TCols> {
  Enabled &_enabled;
  int _row;
  int _col;
  const char *_chars;
  int _idx;
public:
  Spinner(int row, int col, Enabled &enabled, const char *chars = "* "): _row(row), _col(col), _enabled(enabled), _idx(0), _chars(chars) {}
  void draw(DisplayBuffer<TRows, TCols> &buffer) {
    if (_enabled.enabled()) {
      buffer.set(_row, _col, _chars[_idx]);
      _idx = (_idx + 1) % (int)strlen(_chars);
    } else {
      buffer.set(_row, _col, ' ');
    }
  }
};

template <class TDisplay, class TValue, int TRows = 4, int TCols = 20>
class DisplayValue : public DisplayDrawable<TDisplay, TRows, TCols> {
  int _row;
  int _col;
  const char *_before;
  TValue &_value;
  const char *_after;
  int _width;
public:
  DisplayValue(int row, int col, const char *before, TValue &value, const char *after):
    _row(row), _col(col), _before(before), _value(value), _after(after), _width(0) {}
  DisplayValue(int row, int col, const char *before, TValue &value, const char *after, int width):
    _row(row), _col(col), _before(before), _value(value), _after(after), _width(width) {}
  void draw(DisplayBuffer<TRows, TCols> &buffer) {
		char temp[32];
		temp[0] = 0;
		int offset = 0;
		if (_before) {
			buffer.write(_row, _col + offset, _before);
			offset += (int)strlen(_before);
		}
		snprintf(temp, sizeof(temp), "%ld", (long)_value);
		int valueWidth = _width;
		if (valueWidth <= 0) valueWidth = (int)strlen(temp);
		buffer.write(_row, _col + offset, temp, valueWidth);
		offset += valueWidth;
		if (_after) {
			buffer.write(_row, _col + offset, _after);
		}
  }
};

// You need to include <Arduino_JSON.h> for this to work.
#ifdef _ARDUINO_JSON_H_
template <class TDisplay, int TRows = 4, int TCols = 20>
class SerialJsonBug : public DisplayDrawable<TDisplay, TRows, TCols>, private Scheduled {
  JSONVar _json;
  Timer _timeout;
public:
  SerialJsonBug(Schedule &schedule, long period) : Scheduled(schedule), _timeout(period) { }
  void poll() {
    if (Serial.available()) {
      _json = JSON.parse(Serial.readString());
      _json["dirty"] = true;
      _timeout.reset();
    }
  }
  void draw(DisplayBuffer<TRows, TCols> &buffer) {
    if (!_json["dirty"]) {
      if (_timeout.expired()) {
 		buffer.clear(' ');
 		buffer.write(0, 0, "No data");
        _timeout.reset();
      }
      return; // reduce flickering
    }
    _json["dirty"] = false;
    _timeout.reset();
    update(buffer);
  }
  virtual void update(DisplayBuffer<TRows, TCols> &buffer) = 0;
protected:
  JSONVar &json_data() { return _json; }
};
#endif

/* Not sure what to do with space invaders... yet.
https://www.ibbotson.co.uk/software/invaders/2020/12/22/space-invaders-charactermaps.html
uint8_t space_1_a_left[8] = {
  0b00011,
  0b01111,
  0b11100,
  0b11111,
  0b00110,
  0b01100,
  0b11000,
};
uint8_t space_1_a_right[8] = {
  0b11000,
  0b11110,
  0b00111,
  0b11111,
  0b01100,
  0b00110,
  0b00011,
};
uint8_t space_1_b_left[8] = {
  0b00011,
  0b01111,
  0b11100,
  0b11111,
  0b00110,
  0b01101,
  0b00110,
};
uint8_t space_1_b_right[8] = {
  0b11000,
  0b11110,
  0b00111,
  0b11111,
  0b01100,
  0b10110,
  0b01100,
};
*/

#endif // DISPLAY_HPP
