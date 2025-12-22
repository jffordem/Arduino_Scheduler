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
#include <Graphics.hpp>

template <class TDisplay>
class MainDisplay : private Scheduled, private Timer {
    TDisplay &_display;
    Drawable<TDisplay> &_drawable;
public:
    MainDisplay(Schedule &schedule, TDisplay &display, Drawable<TDisplay> &drawable, long period) : 
        Scheduled(schedule), Timer(period), _display(display), _drawable(drawable) { }
    void begin() {
        reset();
        _display.begin();
        _display.backlight();
        _display.clear();
        _display.home();
        _display.noCursor();
    }
    void poll() override {
        if (expired()) {
            reset();
            _drawable.draw(_display);
        }
    }
};

template <class TDisplay>
class DisplayLabel : public Drawable<TDisplay> {
    int _row;
    int _col;
    const char *_text;
public:
    DisplayLabel(int row, int col, const char *text) : _row(row), _col(col), _text(text) { }
    void draw(TDisplay &lcd) {
        lcd.setCursor(_col, _row);
        lcd.print(_text);
    }
};

template <class TDisplay>
class Spinner : public Drawable<TDisplay> {
  Enabled &_enabled;
  int _row;
  int _col;
  const char *_chars;
  int _idx;
public:
  Spinner(int row, int col, Enabled &enabled, const char *chars = "* "): _row(row), _col(col), _enabled(enabled), _idx(0), _chars(chars) {}
  void draw(TDisplay &lcd) {
    lcd.setCursor(_col, _row);
    if (_enabled.enabled()) {
      lcd.print(_chars[_idx]);
      _idx = (_idx + 1) % strlen(_chars);
    } else {
      lcd.print(" ");
    }
  }
};

template <class TDisplay, class TValue>
class DisplayValue : public Drawable<TDisplay> {
  int _row;
  int _col;
  const char *_before;
  TValue &_value;
  const char *_after;
public:
  DisplayValue(int row, int col, const char *before, TValue &value, const char *after):
    _row(row), _col(col), _before(before), _value(value), _after(after) {}
  void draw(TDisplay &lcd) {
    lcd.setCursor(_col, _row);
    if (_before) lcd.print(_before);
    lcd.print(_value);
    if (_after) lcd.print(_after);
  }
};

// You need to include <Arduino_JSON.h> for this to work.
#ifdef _ARDUINO_JSON_H_
template <class TDisplay>
class SerialJsonBug : public Drawable<TDisplay>, private Scheduled {
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
  void draw(TDisplay &lcd) {
    if (!_json["dirty"]) {
      if (_timeout.expired()) {
        lcd.clear();
        lcd.home();
        lcd.print("No data");
        _timeout.reset();
      }
      return; // reduce flickering
    }
    _json["dirty"] = false;
    _timeout.reset();
    update(lcd);
  }
  virtual void update(TDisplay &lcd) = 0;
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
