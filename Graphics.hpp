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

// Adafruit_GFX
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
#ifndef USE_VA_ARGS
    template <class... Args>
	PressComposite(Pressable *first, Args... rest) :
		Composite<Pressable>(first, rest...) { }
#endif
  void draw(TDisplay &display) {
    const int count = length();
		for (int i = 0; i < count; i++) {
			item(i)->draw(display);
		}
	}
};

template <class TDisplay>
class MainWindow : public Clock, private EdgeDetectorBase {
  TDisplay &_display; // Adafruit_SSD1306
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
  void onFallingEdge() {
    // Serial.println("MAINWINDOW FALLING EDGE");
  }
};

/*
class Bitmap : private Drawn {
  Rect &_rect;
  const uint8_t *_image;
  uint16_t _color;
public:
  Bitmap(Adafruit_GFX &display, const uint8_t *image, Rect &rect, uint16_t color = SSD1306_WHITE) :
    Drawn(display), _rect(rect), _image(image), _color(color) { }
  void draw() { _display.drawBitmap(_rect.x, _rect.y, _image, _rect.width, _rect.height, _color); }
};
*/

// Adafruit_GFX
/*
template <class TDisplay>
class VirtualLED : public Drawable<TDisplay> {
  int16_t &_x;
  int16_t &_y;
  const int16_t &_radius;
  const uint16_t _color;
  bool &_ledState;
public:
  VirtualLED(MainWindow<TDisplay> &window, bool &ledState, int16_t &x, int16_t &y, int16_t &radius, uint16_t color = SSD1306_WHITE) :
    _ledState(ledState), _x(x), _y(y), _radius(radius), _color(color) { window.add(this); }
  void draw(TDisplay &display) {
    if (_ledState) {
      display.fillCircle(_x, _y, _radius, _color);
    } else {
      display.drawCircle(_x, _y, _radius, _color);
    }
  }
};

class Label : public Drawable {
  Pos &_pos;
  String _text;
  uint8_t _textSize;
  uint16_t _fgColor;
  uint16_t _bgColor;
public:
  Label(Adafruit_GFX &display, Pos &pos, const String text = "", uint8_t textSize = 1, uint16_t fgColor = SSD1306_WHITE, uint16_t bgColor = SSD1306_WHITE) :
    Drawable(display), _textSize(textSize), _fgColor(fgColor), _bgColor(bgColor), _text(text), _pos(pos) { }
  void setTextSize(uint8_t textSize) { _textSize = textSize; }
  void setTextColor(uint16_t fg, uint16_t bg) { _fgColor = fg; _bgColor = bg; }
  void draw() {
    _display.setTextSize(_textSize);
    _display.setTextColor(_fgColor, _bgColor);
    _display.setCursor(_pos.x, _pos.y);
    _display.print(_text);
  }
  void setText(const String &text) { _text = text; }
};

class DigitalClockFace : public Label, private Clock, private EdgeDetectorBase {
  bool _clkValue;
  long _halfSecond = 500;
public:
  DigitalClockFace(Schedule &schedule, Adafruit_GFX &display, Pos &pos, uint8_t textSize = 4) :
    Label(display, pos, "12:00", textSize, SSD1306_WHITE, SSD1306_WHITE),
    Clock(schedule, _halfSecond, _halfSecond, _clkValue),
    EdgeDetectorBase(schedule, _clkValue) { }
    void onFallingEdge() { }
    void onRisingEdge() { updateTime(); }
    // void updateTime() { setText(String(millis())); }
    void updateTime() { setText("12:01") ;}
};

// Maybe leverage some of their code?
// Not sure how you emit/change the state.
class GFXButton : public Drawable, private Adafruit_GFX_Button, private Scheduled {
  EdgeDetector _detector;
  char _buffer[32];
  bool _btnState;
public:
  GFXButton(Schedule &schedule, Adafruit_GFX &display, const Rect &rect, const char *text, void (*onButtonPressed)()) :
    Drawable(display),
    Scheduled(schedule), _detector(schedule, _btnState, onButtonPressed) {
    strcpy(_buffer, text);
    initButtonUL(&display, rect.x, rect.y, rect.width, rect.height, SSD1306_WHITE, SSD1306_WHITE, SSD1306_WHITE, _buffer, 1);
  }
  void draw() {
    drawButton();
  }
  void poll() {
    _btnState = isPressed();
  }
};
*/

/*
class Turtle : public Drawable {
protected:
  DrawAt &_image;
  const Size &_size;
  Pos _pos;
public:
  Turtle(DrawAt &image, const Size &size) : _image(image), _size(size) { }
  void draw(Adafruit_SSD1306 &display) { _image.draw(display, _pos); }
  virtual void begin(const Pos &initialPos, const Rect &bounds) = 0;
  virtual void move(const Rect &bounds) = 0;
};

class Snowflake : public Turtle, private Timer {
  int16_t _dy;
public:
  Snowflake(DrawAt &image, const Size &size) : Turtle(image, size) { }
  void begin(const Pos &initialPos, const Rect &bounds) {
    start(bounds);
    reset(200);
  }
  void start(const Rect &bounds) {
    _pos.x = random(1 - _size.width + bounds.left(), bounds.right());
    _pos.y = -_size.height + bounds.top();
    _dy = random(1, 6);
  }
  void move(const Rect &bounds) {
    if (expired()) {
      _pos.y += _dy;
      if (_pos.y >= bounds.bottom()) {
        start(bounds);
      }
      reset(200);
    }
  }
};
*/

#endif // GRAPHICS_HPP
