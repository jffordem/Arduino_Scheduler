#ifndef _ILCD_h
#define _ILCD_h

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

/* LCD Interface provides a common base for various LCD displays.
 * An LCD display should implement Print, and basic commands such as clear, home, etc.
 */

class ILCD : public Print {
public:
    virtual void begin() = 0;
    virtual void clear() = 0;
    virtual void home() = 0;
    virtual void noDisplay() = 0;
    virtual void display() = 0;
    virtual void noBlink() = 0;
    virtual void blink() = 0;
    virtual void noCursor() = 0;
    virtual void cursor() = 0;
    virtual void scrollDisplayLeft() = 0;
    virtual void scrollDisplayRight() = 0;
    virtual void leftToRight() = 0;
    virtual void rightToLeft() = 0;
    virtual void noBacklight() = 0;
    virtual void backlight() = 0;
    virtual bool getBacklight() = 0;
    virtual void autoscroll() = 0;
    virtual void noAutoscroll() = 0;
	virtual void setCursor(uint8_t, uint8_t);
};

class IKeypad {
public:
    virtual void begin() = 0;
    virtual uint8_t read() = 0;
    virtual void clear() = 0;
};

#endif // _ILCD_h