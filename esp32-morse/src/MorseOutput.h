#pragma once

class MorseOutput {
public:
    virtual void begin() = 0;
    virtual void mark()  = 0;   // signal on  (LED white / buzzer tone)
    virtual void space() = 0;   // signal off
    virtual ~MorseOutput() = default;
};

// WS2812B on GPIO48 (built-in RGB LED on ESP32-S3-DevKitC-1).
// In dev builds (no MORSE_ACTIVE) mark()/space() log to Serial instead.
class LedOutput : public MorseOutput {
public:
    void begin() override;
    void mark()  override;
    void space() override;
};
