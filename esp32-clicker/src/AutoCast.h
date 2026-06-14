#pragma once
#include "ClickerMode.h"
#include "HID.h"
#include <Arduino.h>

// Repeatedly presses and releases a key or mouse button on configurable timing.
// Params: "input" ("left"/"right"/"middle" or single char key),
//         "holdMs" (how long to hold down), "waitMs" (gap between casts).
class AutoCast : public ClickerMode {
    enum class State : uint8_t { IDLE, HOLDING, WAITING };

    State    _state   = State::IDLE;
    uint32_t _stateAt = 0;

    bool    _isMouse  = true;
    uint8_t _key      = 'r';
    uint8_t _mouseBtn = MOUSE_LEFT;

    uint32_t _holdMs = 1200;
    uint32_t _waitMs = 800;

    void pressAction()   {
        if (_isMouse) HID::pressMouse(_mouseBtn);
        else          HID::pressKey(_key);
    }
    void releaseAction() {
        if (_isMouse) HID::releaseMouse(_mouseBtn);
        else          HID::releaseKey(_key);
    }

public:
    const char* name()    const override { return "AutoCast"; }
    bool        running() const override { return _state != State::IDLE; }

    void start() override {
        pressAction();
        _state   = State::HOLDING;
        _stateAt = millis();
    }

    void stop() override {
        if (_state == State::HOLDING) releaseAction();
        _state = State::IDLE;
    }

    void poll(uint32_t now) override {
        switch (_state) {
            case State::IDLE: break;
            case State::HOLDING:
                if (now - _stateAt >= _holdMs) {
                    releaseAction();
                    _state   = State::WAITING;
                    _stateAt = now;
                }
                break;
            case State::WAITING:
                if (now - _stateAt >= _waitMs) {
                    pressAction();
                    _state   = State::HOLDING;
                    _stateAt = now;
                }
                break;
        }
    }

    void setParam(const char* k, const char* v) override {
        if (strcmp(k, "input") == 0) {
            if      (strcmp(v, "left")   == 0) { _isMouse = true;  _mouseBtn = MOUSE_LEFT;   }
            else if (strcmp(v, "right")  == 0) { _isMouse = true;  _mouseBtn = MOUSE_RIGHT;  }
            else if (strcmp(v, "middle") == 0) { _isMouse = true;  _mouseBtn = MOUSE_MIDDLE; }
            else if (v[0])                     { _isMouse = false; _key      = (uint8_t)v[0]; }
        } else if (strcmp(k, "holdMs") == 0) {
            _holdMs = (uint32_t)atol(v);
        } else if (strcmp(k, "waitMs") == 0) {
            _waitMs = (uint32_t)atol(v);
        }
    }

    String statusJson() const override {
        char input[8];
        if (_isMouse) {
            strcpy(input, _mouseBtn == MOUSE_LEFT  ? "left"  :
                          _mouseBtn == MOUSE_RIGHT ? "right" : "middle");
        } else {
            input[0] = (char)_key; input[1] = '\0';
        }
        char buf[128];
        snprintf(buf, sizeof(buf),
            "{\"mode\":\"AutoCast\",\"running\":%s"
            ",\"input\":\"%s\",\"holdMs\":%u,\"waitMs\":%u}",
            running() ? "true" : "false", input, _holdMs, _waitMs);
        return String(buf);
    }
};
