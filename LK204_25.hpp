#ifndef LK204_25_HPP
#define LK204_25_HPP

#include <ILCD.h>

/* LCD Driver for LK204-25 LCD Display
 * Requires Wire library
 * 
 * Example:
 * #include <Wire.h>
 * #include <LK204_25.hpp>
 * LK204_25 lcd;
 * void setup() {
 *   lcd.begin();
 *   lcd.print("Hello, World!");
 * }
 * void loop() { }
*/

class LK204_25_Base {
    static const uint8_t CommandPrefix = 0xFE;
    static const uint8_t WriteDelay = 1;
    static const uint8_t CommandDelay = 5 - WriteDelay;
    const uint8_t _address;

public:
    LK204_25_Base(uint8_t addr) : _address(addr) { }

protected:
    uint8_t getAddr() { return _address; }
    void send_command(uint8_t cmd) {
        Wire.beginTransmission(_address);
        Wire.write(CommandPrefix);
        Wire.write(cmd);
        Wire.endTransmission();
        delay(CommandDelay);
    }

    void send_command_2(uint8_t cmd, uint8_t val) {
        Wire.beginTransmission(_address);
        Wire.write(CommandPrefix);
        Wire.write(cmd);
        Wire.write(val);
        Wire.endTransmission();
        delay(CommandDelay);
    }

    void send_command_3(uint8_t cmd, uint8_t arg1, uint8_t arg2) {
        Wire.beginTransmission(_address);
        Wire.write(CommandPrefix);
        Wire.write(cmd);
        Wire.write(arg1);
        Wire.write(arg2);
        Wire.endTransmission();
        delay(CommandDelay);
    }

    void send_byte(uint8_t b) {
        Wire.beginTransmission(_address);
        Wire.write(b);
        Wire.endTransmission();
        delay(WriteDelay);
    }
};

class LK204_25_LCD : public ILCD, private LK204_25_Base {
    static const uint8_t DefaultAddress = 0x2E;
    static const uint8_t Command_Home = 'H';
    static const uint8_t Command_Clear = 'X';
    static const uint8_t Command_LineWrap = 'C';
    static const uint8_t Command_NoLineWrap = 'D';
    static const uint8_t Command_AutoScroll = 'Q';
    static const uint8_t Command_NoAutoScroll = 'R';
    static const uint8_t Command_Backspace = 'L';
    static const uint8_t Command_Backlight = 'B';
    static const uint8_t Command_NoBacklight = 'F';
    static const uint8_t Command_SetCursor = 'G';
    static const uint8_t Command_CursorUnderline = 'J';
    static const uint8_t Command_NoCursorUnderline = 'K';
    static const uint8_t Command_CursorBlock = 'S';
    static const uint8_t Command_NoCursorBlock = 'T';
    const uint8_t _cols;
    const uint8_t _rows;
public:
    LK204_25_LCD(uint8_t lcd_addr = DefaultAddress, int cols = 20, int rows = 4) : LK204_25_Base(lcd_addr), _cols(cols), _rows(rows) { }
    void begin() {
        Wire.begin();
        lineWrap(true);
        autoScroll(true);
        clear();
        home();
    }
    void home() { send_command(Command_Home); }
    void clear() { send_command(Command_Clear); }
    void lineWrap(bool on) { send_command(on ? Command_LineWrap : Command_NoLineWrap); }
    void autoScroll(bool on) { send_command(on ? Command_AutoScroll : Command_NoAutoScroll); }
    void backspace() { send_command(Command_Backspace); }
    void backlight_on() { send_command_2(Command_Backlight, 0); }
    void backlight_off() { send_command(Command_NoBacklight); }
    void setCursor(uint8_t col, uint8_t row) { send_command_3(Command_SetCursor, col+1, row+1); }
    void cursorUnderline(bool on) { send_command(on ? Command_CursorUnderline : Command_NoCursorUnderline); }
    void cursorBlock(bool on) { send_command(on ? Command_CursorBlock : Command_NoCursorBlock); }
	void blink_on() { cursorBlock(true); }
	void blink_off() { cursorBlock(false); }
	void cursor_on() { blink_on(); }
	void cursor_off() { cursorBlock(false); cursorUnderline(false); }
    void autoscroll() { }
    void noAutoscroll() { }
    void scrollDisplayLeft() { }
    void scrollDisplayRight() { }
    void printLeft() { }
    void printRight() { }
    void leftToRight() { }
    void rightToLeft() { }
    void shiftIncrement() { }
    void shiftDecrement() { }
    void display() { }
    void noDisplay() { }
    void blink() { blink_on(); }
    void noBlink() { blink_off(); }
    void cursor() { cursor_on(); }
    void noCursor() { cursor_off(); }
    void backlight() { backlight_on(); }
    bool getBacklight() { return true; }
    void noBacklight() { backlight_off(); }

    virtual size_t write(uint8_t c) { send_byte(c); return 1; }

    int getCols() { return _cols; }
    int getRows() { return _rows; }
};

/* Keypad Driver for LK204-25 LCD Display
 * Requires Wire library
 * The keypad is a 4x4 matrix with the following layout:
 * A B C D
 * F G H I
 * K L M N
 * P Q R S
 */
class LK204_25_Keypad : public IKeypad, private LK204_25_Base {
    static const uint8_t DefaultAddress = 0x2E;
    static const uint8_t Command_AutoRepeatMode = '~';
    static const uint8_t Command_NoAutoRepeatMode = '`';
    static const uint8_t Command_AutoTransmitMode = 'A';
    static const uint8_t Command_NoAutoTransmitMode = 'O';
    static const uint8_t Command_ClearBuffer = 'E';
    static const uint8_t Command_ReadKey = '&';
    static const uint8_t Command_SetDebounce = 'U';

public:
    LK204_25_Keypad(uint8_t keypad_addr = DefaultAddress) : LK204_25_Base(keypad_addr) { }
    void begin() { Wire.begin(); }
    void clear() { send_command(Command_ClearBuffer); }
    uint8_t read() {
        const uint8_t quantity = 1;
        // send_command(Command_ReadKey);
        Wire.requestFrom(getAddr(), quantity);
        return Wire.read();
    }
};

#endif // LK204_25_HPP
