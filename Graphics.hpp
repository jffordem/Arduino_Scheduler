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

#ifndef GRAPHICS_HPP
#define GRAPHICS_HPP

#include <Clock.hpp>
#include <EdgeDetector.hpp>

template <class T>
struct Pos {
  Pos(T xval = 0, T yval = 0) : x(xval), y(yval) { }
  Pos<T> &operator = (const Pos<T> &pos) { x = pos.x; y = pos.y; return *this; }
  T x;
  T y;
};

template <class T>
struct Size {
  Size(T w = 0, T h = 0) : width(w), height(h) { }
  Size<T> &operator = (const Size<T> &size) { width = size.width; height = size.height; return *this; }
  T width;
  T height;
};

template <class T>
struct Rect : public Pos<T>, public Size<T> {
  Rect(T x = 0, T y = 0, T width = 0, T height = 0) : Pos<T>(x, y), Size<T>(width, height) { }
  Rect<T> &operator = (const Rect<T> &rect) { this->x = rect.x; this->y = rect.y; this->width = rect.width; this->height = rect.height; return *this; }
  T left() const { return this->x; }
  T top() const { return this->y; }
  T right() const { return this->x + this->width; }
  T bottom() const { return this->y + this->height; }
};

template <class TDisplay>
class Drawable {
public:
  virtual void draw(TDisplay &display) = 0;
};

template <class TDisplay>
class DrawableComposite : public Composite<Drawable<TDisplay>> {
public:
  DrawableComposite(Drawable<TDisplay> *itemsZ[] = NULL) :
    Composite<Drawable<TDisplay>>(itemsZ, countZ(itemsZ)) { }
#ifdef USE_VA_ARGS
  template <class... Args>
  DrawableComposite(Drawable<TDisplay> *first, Args... rest) :
    Composite<Drawable<TDisplay>>(first, rest...) { }
#endif
  void draw(TDisplay &display) {
    const int count = this->length();
    for (int i = 0; i < count; i++) {
      this->item(i)->draw(display);
    }
  }
};

// Drives an Adafruit_GFX-compatible display at a fixed refresh rate.
// Add Drawable objects via add(); they are drawn in registration order each tick.
template <class TDisplay>
class MainWindow : public Clock, private EdgeDetectorBase {
  TDisplay &_display;
  long _clockHigh = 25;
  long _clockLow = 25;
  bool _clockValue;
  List<Drawable<TDisplay>*> _items;
public:
  MainWindow(Schedule &schedule, TDisplay &display) :
    Clock(schedule, _clockLow, _clockHigh, _clockValue),
    EdgeDetectorBase(schedule, _clockValue), _display(display) { }
  void add(Drawable<TDisplay> *item) { _items.add(item); }
  void update() {
    _display.clearDisplay();
    for (int i = 0; i < _items.length(); i++) {
      _items[i]->draw(_display);
    }
    _display.display();
  }
  void onRisingEdge() { update(); }
  void onFallingEdge() { }
};

#endif // GRAPHICS_HPP
