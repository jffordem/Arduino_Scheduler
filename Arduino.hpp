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

#if DEBUG == 1
#define DEBUG_INIT() Serial.begin(115200)
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINT2(x, d) Serial.print(x, d)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTLN2(x, d) Serial.println(x, d)
#else
#define DEBUG_INIT()
#define DEBUG_PRINT(x)
#define DEBUG_PRINT2(x, d)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTLN2(x, d)
#endif


// Want to define these so that I don't get a bunch of lint issues
// in the editor.  Taking advantage of the fact that arduino defines
// max as a macro.
#ifdef COMMENTEDOUT

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define constrain(x, a, b) (max((a), min((x), (b))))
#define random(a, b) (0)
void delay(long ms) { }
long int millis() { return 0; }
void tone(uint8_t,uint16_t) { }
void noTone(uint8_t) { }
#define INPUT_PULLUP 1
#define INPUT 1
#define OUTPUT 1
#define HIGH 1
#define LOW 0
#define NULL 0
#define LED_BUILTIN 1
#define RISING 1
#define FALLING 1
void pinMode(int i, int j) { }
bool digitalRead(int i) { return LOW; }
int digitalPinToInterrupt(int i) { return 0; }
void attachInterrupt(int i, void (*j)(), int k) { }
void digitalWrite(int i, bool j) { }
int analogRead(int i) { return 0; }
void analogWrite(int i, int j) { }
void strcpy(char*, const char*) { }
#define DEC 1
class Outputter {
public:
	bool available() { }
	String readString() { }
	void print(const char*) { }
	void print(long) { }
	void print(long,short) {}
	void print(short,short) {}
	void print(bool,short) {}
	void print(short) { }
	void println(const char*) { }
	void println(int,int) {}
	void println() { }
	void println(long) { }
	void println(short) { }
};
Outputter Serial;
class PressyThing {
public:
	void press(int) { }
	void release(int) { }
	void releaseAll() { }
	void move(int,int,int) { }
};
#define KEY_F5 123
PressyThing Keyboard;
PressyThing Mouse;
typedef int int16_t;
typedef unsigned int uint16_t;
typedef char int8_t;
typedef unsigned char uint8_t;
typedef char GFXfont;
class Adafruit_GFX {
public:
	void drawBitmap(int16_t,int16_t,const uint8_t *,int16_t,int16_t,uint16_t) { }
	void setFont(const GFXfont *) { }
	void setTextSize(uint8_t) { }
	void setTextColor(uint16_t,uint16_t) { }
	void setCursor(int16_t,int16_t) { }
	void print(const String&) { }
	void drawCircle(int16_t,int16_t,int16_t,uint16_t) { }
	void fillCircle(int16_t,int16_t,int16_t,uint16_t) { }
};
class Adafruit_SSD1306 : public Adafruit_GFX {
public:
	void clearDisplay() { }
	void display() { }
};
class Adafruit_GFX_Button {

public:
  Adafruit_GFX_Button(void);
  // "Classic" initButton() uses center & size
  void initButton(Adafruit_GFX *gfx, int16_t x, int16_t y, uint16_t w,
                  uint16_t h, uint16_t outline, uint16_t fill,
                  uint16_t textcolor, char *label, uint8_t textsize);
  void initButton(Adafruit_GFX *gfx, int16_t x, int16_t y, uint16_t w,
                  uint16_t h, uint16_t outline, uint16_t fill,
                  uint16_t textcolor, char *label, uint8_t textsize_x,
                  uint8_t textsize_y);
  // New/alt initButton() uses upper-left corner & size
  void initButtonUL(Adafruit_GFX *gfx, int16_t x1, int16_t y1, uint16_t w,
                    uint16_t h, uint16_t outline, uint16_t fill,
                    uint16_t textcolor, char *label, uint8_t textsize);
  void initButtonUL(Adafruit_GFX *gfx, int16_t x1, int16_t y1, uint16_t w,
                    uint16_t h, uint16_t outline, uint16_t fill,
                    uint16_t textcolor, char *label, uint8_t textsize_x,
                    uint8_t textsize_y);
  void drawButton(bool inverted = false);
  bool contains(int16_t x, int16_t y);
  bool isPressed() { }
};
class String {
public:
	String() { }
	String(const char*) { }
	String substring(int) { }
	String &operator = (const char*) { return *this; }
	String operator + (const char*) { }
	operator const char* () { return NULL; }
	void trim() { }
};
uint16_t SSD1306_WHITE = 0;
#endif

#endif // COMMENTEDOUT