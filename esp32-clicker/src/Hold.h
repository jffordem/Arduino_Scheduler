#pragma once
#include "ClickerMode.h"
#include "HID.h"
#include <Arduino.h>

// Holds a mouse button or key down until stopped.
// start() presses, stop() releases — no timing, no polling.
class Hold : public ClickerMode {
    bool    _isMouse  = true;
    uint8_t _key      = 'r';
    uint8_t _mouseBtn = MOUSE_LEFT;
    bool    _running  = false;

    void pressAction() {
        if (_isMouse) HID::pressMouse(_mouseBtn);
        else          HID::pressKey(_key);
    }
    void releaseAction() {
        if (_isMouse) HID::releaseMouse(_mouseBtn);
        else          HID::releaseKey(_key);
    }

public:
    const char* name()    const override { return "Hold"; }
    bool        running() const override { return _running; }

    void start() override {
        pressAction();
        _running = true;
    }

    void stop() override {
        releaseAction();
        _running = false;
    }

    void poll(uint32_t) override {}

    void setParam(const char* k, const char* v) override {
        if (strcmp(k, "input") != 0) return;
        if      (strcmp(v, "left")   == 0) { _isMouse = true;  _mouseBtn = MOUSE_LEFT;   }
        else if (strcmp(v, "right")  == 0) { _isMouse = true;  _mouseBtn = MOUSE_RIGHT;  }
        else if (strcmp(v, "middle") == 0) { _isMouse = true;  _mouseBtn = MOUSE_MIDDLE; }
        else if (v[0])                     { _isMouse = false; _key      = (uint8_t)v[0]; }
    }

    String statusJson() const override {
        char input[8];
        if (_isMouse) {
            strcpy(input, _mouseBtn == MOUSE_LEFT  ? "left"  :
                          _mouseBtn == MOUSE_RIGHT ? "right" : "middle");
        } else {
            input[0] = (char)_key; input[1] = '\0';
        }
        char buf[80];
        snprintf(buf, sizeof(buf),
            "{\"mode\":\"Hold\",\"running\":%s,\"input\":\"%s\"}",
            _running ? "true" : "false", input);
        return String(buf);
    }
};
