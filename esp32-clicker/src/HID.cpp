#include "HID.h"

#ifdef USB_HID_MODE
static USBHIDKeyboard _kb;
static USBHIDMouse    _ms;
#endif

namespace HID {

void begin() {
#ifdef USB_HID_MODE
    _kb.begin();
    _ms.begin();
    USB.begin();
#endif
}

void pressKey(uint8_t code) {
#ifdef USB_HID_MODE
    _kb.press(code);
#else
    Serial.printf("[HID] KEY PRESS   0x%02X\n", code);
#endif
}

void releaseKey(uint8_t code) {
#ifdef USB_HID_MODE
    _kb.release(code);
#else
    Serial.printf("[HID] KEY RELEASE 0x%02X\n", code);
#endif
}

void tapKey(uint8_t code) {
    pressKey(code);
    delay(20);
    releaseKey(code);
}

void pressMouse(uint8_t button) {
#ifdef USB_HID_MODE
    _ms.press(button);
#else
    Serial.printf("[HID] MOUSE PRESS   %s\n",
        button == MOUSE_LEFT ? "LEFT" : button == MOUSE_RIGHT ? "RIGHT" : "MIDDLE");
#endif
}

void releaseMouse(uint8_t button) {
#ifdef USB_HID_MODE
    _ms.release(button);
#else
    Serial.printf("[HID] MOUSE RELEASE %s\n",
        button == MOUSE_LEFT ? "LEFT" : button == MOUSE_RIGHT ? "RIGHT" : "MIDDLE");
#endif
}

void releaseAll() {
#ifdef USB_HID_MODE
    _kb.releaseAll();
    _ms.release(MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE);
#else
    Serial.println("[HID] RELEASE ALL");
#endif
}

} // namespace HID
