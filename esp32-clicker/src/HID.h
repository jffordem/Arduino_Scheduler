#pragma once
#include <Arduino.h>

#ifdef USB_HID_MODE
#  include "USB.h"
#  include "USBHIDKeyboard.h"
#  include "USBHIDMouse.h"
#endif

// Mouse button bitmasks — match arduino-esp32 USBHIDMouse values.
#ifndef MOUSE_LEFT
#  define MOUSE_LEFT   0x01
#  define MOUSE_RIGHT  0x02
#  define MOUSE_MIDDLE 0x04
#endif

namespace HID {
    void begin();
    void pressKey(uint8_t code);
    void releaseKey(uint8_t code);
    void tapKey(uint8_t code);        // press + 20 ms + release
    void pressMouse(uint8_t button);
    void releaseMouse(uint8_t button);
    void releaseAll();
}
