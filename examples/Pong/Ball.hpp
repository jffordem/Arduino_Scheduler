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
#include <Clock.hpp>
#include <Graphics.hpp>
#include "Paddle.hpp"

class Ball : public Drawable, private Scheduled {
  int16_t _x;
  int16_t _y;
  int16_t _radius;
  int16_t _dx;
  int16_t _dy;
  int16_t _dt;
  int16_t _width;
  int16_t _height;
  Timer _timer;
  int16_t _score1;
  int16_t _score2;
  Paddle &_player1;
  Paddle &_player2;
public:
  Ball(Schedule &schedule, MainWindow &window, Paddle &player1, Paddle &player2, int16_t width, int16_t height) :
    Scheduled(schedule), _x(width >> 1), _y(height >> 1), _width(width), _height(height),
    _player1(player1), _player2(player2), _radius(2), _dx(3), _dy(2), _dt(100) {
      window.add(this);
      _timer.reset(_dt);
      newgame();
    }
  void newgame() {
    _score1 = _score2 = 0;
    newball();
  }
  void newball() {
#ifdef DEBUG
    _dx = 1;
    _dy = 1;
#else
    _dx = randomize(3) * randsign();
    _dy = randomize(2) * randsign();
#endif
    _x = _width >> 1;
    _y = _height >> 1;
  }
  void draw(Adafruit_GFX &display) {
    display.fillCircle(_x, _y, _radius, SSD1306_WHITE);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    int16_t q = _width >> 2;
    display.setCursor(q, 0);
    display.print(_score1, DEC);
    display.setCursor(3*q, 0);
    display.print(_score2, DEC);
  }
  void poll() {
    if (_timer.expired()) {
      _timer.reset(_dt);
      _x += _dx;
      _y += _dy;

      // Wall hit Tests
      if (hittest(-MAX_INT, _height - _radius, MAX_INT, MAX_INT)) {
        _dy = -abs(_dy);
        _dx = randomize(_dx);
      }
#ifndef DEBUG
      if (hittest(-MAX_INT, -MAX_INT, MAX_INT, _radius)) {
        _dy = abs(_dy);
        _dy = randomize(_dy);
      }
      // Paddle hit tests
      int16_t dp = 2; // paddle's "depth"
      int16_t px = _player1.x();
      int16_t y0 = _player1.y0();
      int16_t y1 = _player1.y1();
      if (hittest(px - dp, y0, px + dp, y1)) {
        _dx = abs(_dx);
        _dy = randomize(_dy);
      }
      px = _player2.x();
      y0 = _player2.y0();
      y1 = _player2.y1();
      if (hittest(px - dp, y0, px + dp, y1)) {
        _dx = -abs(_dx);
        _dy = randomize(_dy);
      }
      // Point end hit tests
      if (hittest(_width + 5 * _radius, -MAX_INT, MAX_INT, MAX_INT)) {
        _score1++;
        if (_score1 >= 10) newgame();
        else newball();
      }
      if (hittest(-MAX_INT, -MAX_INT, -5 * _radius, MAX_INT)) {
        _score2++;
        if (_score2 >= 10) newgame();
        else newball();
      }
#endif
    }
  }
private:
  bool hittest(int16_t x0, int16_t y0, int16_t x1, int16_t y1) const {
    // a hit is when the bounding box of the ball overlaps any part of the rectangle
    int16_t bx0 = _x - _radius + 1;
    int16_t by0 = _y - _radius + 1;
    int16_t bx1 = _x + _radius - 1;
    int16_t by1 = _y + _radius - 1;
    return overlap(bx0, by0, bx1, by1, min(x0, x1), min(y0, y1), max(x0, x1), max(y0, y1));
  }
  static bool overlap(int16_t ax1, int16_t ay1, int16_t ax2, int16_t ay2, int16_t bx1, int16_t by1, int16_t bx2, int16_t by2) {
#ifdef DEBUG
    bool p = ax1 <= bx2;
    bool q = ax2 >= bx1;
    bool r = ay1 <= by2;
    bool s = ay2 >= by1;
    bool result = p && q && r && s;
#else
    bool result = ax1 <= bx2 && ax2 >= bx1 && ay1 <= by2 && ay2 >= by1;
#endif
#ifdef DEBUG
    if (millis() < 10000) {
      Serial.print("OVERLAP: (");
      Serial.print(ax1, DEC);
      Serial.print(",");
      Serial.print(ay1, DEC);
      Serial.print(")-(");
      Serial.print(ax2, DEC);
      Serial.print(",");
      Serial.print(ay2, DEC);
      Serial.print(") x (");
      Serial.print(bx1, DEC);
      Serial.print(",");
      Serial.print(by1, DEC);
      Serial.print(")-(");
      Serial.print(bx2, DEC);
      Serial.print(",");
      Serial.print(by2, DEC);
      Serial.print(") = ");
      Serial.print(p, DEC);
      Serial.print(" && ");
      Serial.print(q, DEC);
      Serial.print(" && ");
      Serial.print(r, DEC);
      Serial.print(" && ");
      Serial.print(s, DEC);      
      Serial.print(" = ");
      Serial.println(result, DEC);
    }
#endif
    return result;
  }
  // Should figure out a way for momentum to be conserved.
  int16_t randomize(int16_t value) {
    int16_t sign = value < 0 ? -1 : 1;
    int16_t r = random(0, 3) - 1;
    int16_t temp = value + r;
    // Make sure we didn't change direction or stop
    if (temp * value <= 0) return value;
    return min(4, max(temp, -4));
  }
  int16_t randsign() {
    int r = random(0,100) % 2;
    return r == 0 ? 1 : -1;
  }
};