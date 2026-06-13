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

#include <Graphics.hpp>
#include <EncoderWheel.hpp>

class Paddle : public Drawable, private EncoderControl<int16_t> {
  int16_t _x;
  int16_t _y;
  int16_t _size;
public:
  Paddle(Schedule &schedule, MainWindow &window, const EncoderConfig &config, int16_t x, int16_t size, int16_t height) :
    EncoderControl<int16_t>(schedule, config, _y, (x > 5 ? -5 : 5), height),
    _x(x), _y(height >> 1), _size(size) {
    window.add(this);
  }
  void draw(Adafruit_GFX &display) {
    display.drawLine(_x, y0(), _x, y1(), SSD1306_WHITE);
  }
  int16_t x() const { return _x; }
  int16_t y0() const { return _y - (_size >> 1); }
  int16_t y1() const { return _y + (_size >> 1); }
};