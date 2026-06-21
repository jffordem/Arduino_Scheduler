#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include "ClickerMode.h"
#include "ClickerCmd.h"

// Local hardware UI: 4×20 I2C LCD + two push-button encoder wheels.
//
// Encoder A: rotate = cycle modes (stops active mode on switch),
//            press  = toggle start/stop.
// Encoder B: placeholder — rotate and press currently unmapped.
//            Future intent: A press = enable left mouse, B press = enable right mouse.
//
// Pin assignments:
//   Encoder A  CLK=GPIO1  DT=GPIO2  SW=GPIO42
//   Encoder B  CLK=GPIO41  DT=GPIO40  SW=GPIO39
//   I2C LCD    SDA=GPIO8  SCL=GPIO9  addr=0x27
class LocalUI {
public:
    static constexpr uint8_t ENC_A_CLK = 1;
    static constexpr uint8_t ENC_A_DT  = 2;
    static constexpr uint8_t ENC_A_SW  = 42;
    static constexpr uint8_t ENC_B_CLK = 41;
    static constexpr uint8_t ENC_B_DT  = 40;
    static constexpr uint8_t ENC_B_SW  = 39;
    static constexpr uint8_t I2C_SDA   = 8;
    static constexpr uint8_t I2C_SCL   = 9;
    static constexpr uint8_t LCD_ADDR  = 0x27;

    LocalUI(ClickerMode** modes, int modeCount)
        : _modes(modes), _modeCount(modeCount) {}

    void begin() {
        pinMode(ENC_A_CLK, INPUT_PULLUP);
        pinMode(ENC_A_DT,  INPUT_PULLUP);
        pinMode(ENC_A_SW,  INPUT_PULLUP);
        pinMode(ENC_B_CLK, INPUT_PULLUP);
        pinMode(ENC_B_DT,  INPUT_PULLUP);
        pinMode(ENC_B_SW,  INPUT_PULLUP);

        _aClkLast = digitalRead(ENC_A_CLK);
        _bClkLast = digitalRead(ENC_B_CLK);

        Wire.begin(I2C_SDA, I2C_SCL);
        Wire.setClock(400000);

        // Scan I2C bus and auto-detect LCD address.
        uint8_t foundAddr = 0;
        _i2cScanJson = "{\"i2c\":[";
        bool first = true;
        for (uint8_t addr = 0x08; addr < 0x78; addr++) {
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\"0x%02X\"", addr);
                if (!first) _i2cScanJson += ",";
                _i2cScanJson += buf;
                first = false;
                Serial.printf("[I2C] Found device at 0x%02X\n", addr);
                if (!foundAddr) foundAddr = addr;
            }
        }
        _i2cScanJson += "]}";
        if (!foundAddr) {
            foundAddr = LCD_ADDR;
            Serial.printf("[I2C] No devices found — defaulting to 0x%02X\n", LCD_ADDR);
        }

        _lcd = new LiquidCrystal_I2C(foundAddr, 20, 4);
        _lcd->init();
        _lcd->backlight();
        _lcd->clear();
        _lcd->setCursor(0, 0); _lcd->print("ESP32 Clicker       ");
        _lcd->setCursor(0, 1); _lcd->print("WiFi connecting...  ");
    }

    const String& i2cScanJson() const { return _i2cScanJson; }

    // Call after draining cmdQueue so activeMode is current.
    void poll(uint32_t now, ClickerMode* activeMode) {
        tickEncoderA(now, activeMode);
        tickEncoderB(now);

        if (now - _lastLcdMs >= LCD_PERIOD_MS) {
            _lastLcdMs = now;
            updateLcd(activeMode);
        }
    }

private:
    static constexpr uint32_t LCD_PERIOD_MS  = 100;
    static constexpr uint32_t ENC_GAP_MS    = 5;     // min ms between encoder ticks
    static constexpr int32_t  HOLD_MS_MIN   = 50;
    static constexpr int32_t  HOLD_MS_MAX   = 3000;
    static constexpr int32_t  HOLD_MS_STEP  = 50;
    static constexpr int32_t  WAIT_MS_MIN   = 100;
    static constexpr int32_t  WAIT_MS_MAX   = 5000;
    static constexpr int32_t  WAIT_MS_STEP  = 100;

    ClickerMode** _modes;
    int           _modeCount;
    int32_t       _holdMs = 1200;  // tracks AutoCast holdMs; matches its default
    int32_t       _waitMs = 800;   // tracks AutoCast waitMs; matches its default

    LiquidCrystal_I2C* _lcd      = nullptr;
    String             _i2cScanJson;
    uint32_t           _lastLcdMs = 0;
    char               _shadow[4][21] = {};

    bool     _aClkLast = HIGH;
    uint32_t _aEncLast = 0;
    bool     _bClkLast = HIGH;
    uint32_t _bEncLast = 0;

    // Fires true exactly once per confirmed button press (active-low, debounced).
    struct BtnDebounce {
        bool     raw    = HIGH;
        bool     stable = HIGH;
        uint32_t at     = 0;

        bool pressed(uint8_t pin, uint32_t now) {
            bool r = digitalRead(pin);
            if (r != raw) { raw = r; at = now; }
            if ((now - at) >= 20 && r != stable) {
                stable = r;
                return stable == LOW;
            }
            return false;
        }
    } _aSw, _bSw;

    void pushCmd(ClickerCmd cmd) { xQueueSend(cmdQueue, &cmd, 0); }

    void tickEncoderA(uint32_t now, ClickerMode* activeMode) {
        bool clk = digitalRead(ENC_A_CLK);
        if (clk == HIGH && _aClkLast == LOW && (now - _aEncLast) >= ENC_GAP_MS) {
            _aEncLast = now;
            if (digitalRead(ENC_A_DT) == LOW)
                _holdMs = min(HOLD_MS_MAX, _holdMs + HOLD_MS_STEP);
            else
                _holdMs = max(HOLD_MS_MIN, _holdMs - HOLD_MS_STEP);

            ClickerCmd c{}; c.type = ClickerCmd::SET_PARAM;
            strncpy(c.paramKey, "holdMs", sizeof(c.paramKey) - 1);
            snprintf(c.paramVal, sizeof(c.paramVal), "%ld", (long)_holdMs);
            pushCmd(c);
        }
        _aClkLast = clk;

        if (_aSw.pressed(ENC_A_SW, now)) {
            ClickerCmd c{};
            c.type = (activeMode && activeMode->running()) ? ClickerCmd::STOP
                                                           : ClickerCmd::START;
            pushCmd(c);
        }
    }

    void tickEncoderB(uint32_t now) {
        bool clk = digitalRead(ENC_B_CLK);
        if (clk == HIGH && _bClkLast == LOW && (now - _bEncLast) >= ENC_GAP_MS) {
            _bEncLast = now;
            if (digitalRead(ENC_B_DT) == LOW)
                _waitMs = min(WAIT_MS_MAX, _waitMs + WAIT_MS_STEP);
            else
                _waitMs = max(WAIT_MS_MIN, _waitMs - WAIT_MS_STEP);

            ClickerCmd c{}; c.type = ClickerCmd::SET_PARAM;
            strncpy(c.paramKey, "waitMs", sizeof(c.paramKey) - 1);
            snprintf(c.paramVal, sizeof(c.paramVal), "%ld", (long)_waitMs);
            pushCmd(c);
        }
        _bClkLast = clk;
        _bSw.pressed(ENC_B_SW, now);
    }

    void lcdRow(int r, const char* content) {
        if (memcmp(_shadow[r], content, 20) == 0) return;
        memcpy(_shadow[r], content, 20);
        _lcd->setCursor(0, r);
        _lcd->print(content);
    }

    void updateLcd(ClickerMode* activeMode) {
        if (!_lcd) return;
        char row[21];

        snprintf(row, sizeof(row), "%-20s", activeMode ? activeMode->name() : "---");
        lcdRow(0, row);

        snprintf(row, sizeof(row), "%-20s",
                 (activeMode && activeMode->running()) ? "[RUNNING]" : "[STOPPED]");
        lcdRow(1, row);

        snprintf(row, sizeof(row), "H:%4ldms W:%4ldms   ", (long)_holdMs, (long)_waitMs);
        lcdRow(2, row);

        snprintf(row, sizeof(row), "%-20s", WiFi.localIP().toString().c_str());
        lcdRow(3, row);
    }
};
