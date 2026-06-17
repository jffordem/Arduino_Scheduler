#pragma once
#include <Arduino.h>
#include "MorseOutput.h"
#include "MorseChar.h"

// Non-blocking Morse state machine. Call poll(millis()) every loop().
// Farnsworth timing: characters sent at charWpm; inter-char and inter-word
// gaps stretched so the effective word rate equals effectiveWpm.
class MorseEngine {
public:
    MorseEngine(MorseOutput& out)
        : _out(out), _state(IDLE),
          _charWpm(20), _effectiveWpm(10),
          _buf{}, _bufLen(0), _bufPos(0),
          _elemPos(0), _elemSeq(nullptr),
          _markEnd(0), _gapEnd(0),
          _onDone(nullptr)
    {}

    void setCharWpm(int wpm)      { _charWpm      = constrain(wpm, 5, 60); _recalc(); }
    void setEffectiveWpm(int wpm) { _effectiveWpm = constrain(wpm, 5, _charWpm); _recalc(); }
    int  charWpm()      const { return _charWpm; }
    int  effectiveWpm() const { return _effectiveWpm; }
    bool sending()      const { return _state != IDLE; }

    // Queue text to send; starts immediately if idle. Max 127 chars.
    void send(const char* text) {
        strncpy(_buf, text, sizeof(_buf) - 1);
        _buf[sizeof(_buf) - 1] = '\0';
        _bufLen = strlen(_buf);
        _bufPos = 0;
        _elemPos = 0;
        _elemSeq = nullptr;
        _state = NEXT_CHAR;
    }

    void stop() {
        _state = IDLE;
        _out.space();
    }

    // Optional callback fired when the last element of the last char finishes.
    void onDone(void (*cb)()) { _onDone = cb; }

    // When enabled, each mark/space prints its planned duration to Serial.
    // Use this to cross-check timing against a reference decoder (e.g. lcwo.net).
    void enableTimingLog(bool on) { _timingLog = on; }

    void poll(uint32_t now) {
        switch (_state) {

            case IDLE:
                break;

            case NEXT_CHAR: {
                // Skip to the next non-null char sequence.
                while (_bufPos < _bufLen) {
                    char c = _buf[_bufPos++];
                    if (c == ' ') {
                        _gapEnd = now + _wordGapMs;
                        if (_timingLog)
                            Serial.printf("[%lu] SPACE %lums (word-gap)\n", now, _wordGapMs);
                        _state = WORD_GAP;
                        return;
                    }
                    _elemSeq = morseEncode(c);
                    if (_elemSeq) {
                        _elemPos = 0;
                        _state = NEXT_ELEM;
                        return;
                    }
                    // Unknown char — skip silently.
                }
                // Buffer exhausted.
                _state = IDLE;
                if (_onDone) _onDone();
                break;
            }

            case NEXT_ELEM: {
                if (!_elemSeq || _elemSeq[_elemPos] == '\0') {
                    // End of character — inter-char gap (unless last char).
                    if (_bufPos < _bufLen) {
                        _gapEnd = now + _charGapMs;
                        if (_timingLog)
                            Serial.printf("[%lu] SPACE %lums (char-gap)\n", now, _charGapMs);
                        _state = CHAR_GAP;
                    } else {
                        _state = IDLE;
                        if (_onDone) _onDone();
                    }
                    return;
                }
                char elem = _elemSeq[_elemPos++];
                uint32_t markMs = (elem == '-') ? _dahMs : _ditMs;
                if (_timingLog)
                    Serial.printf("[%lu] MARK  %lums (%s)\n", now, markMs, elem == '-' ? "dah" : "dit");
                _out.mark();
                _markStart = now;
                _markEnd   = now + markMs;
                _state = MARK;
                break;
            }

            case MARK:
                if (now >= _markEnd) {
                    _out.space();
                    // Intra-character gap (1 unit) between elements.
                    if (_elemSeq[_elemPos] != '\0') {
                        _gapEnd = now + _unitMs;
                        if (_timingLog)
                            Serial.printf("[%lu] SPACE %lums (intra)\n", now, _unitMs);
                        _state = INTRA_GAP;
                    } else {
                        _state = NEXT_ELEM;
                    }
                }
                break;

            case INTRA_GAP:
                if (now >= _gapEnd) _state = NEXT_ELEM;
                break;

            case CHAR_GAP:
                if (now >= _gapEnd) _state = NEXT_CHAR;
                break;

            case WORD_GAP:
                if (now >= _gapEnd) _state = NEXT_CHAR;
                break;

        }
    }

private:
    enum State { IDLE, NEXT_CHAR, NEXT_ELEM, MARK, INTRA_GAP, CHAR_GAP, WORD_GAP };

    MorseOutput& _out;
    State        _state;
    int          _charWpm;
    int          _effectiveWpm;
    bool         _timingLog = false;

    char         _buf[128];
    int          _bufLen, _bufPos;
    int          _elemPos;
    const char*  _elemSeq;

    uint32_t     _markStart, _markEnd, _gapEnd;
    void         (*_onDone)();

    // Timing (all in ms), recalculated whenever WPM changes.
    uint32_t _unitMs    = 60;   // 1200 / charWpm
    uint32_t _ditMs     = 60;
    uint32_t _dahMs     = 180;
    uint32_t _charGapMs = 180;
    uint32_t _wordGapMs = 420;

    void _recalc() {
        _unitMs    = 1200 / _charWpm;
        _ditMs     = _unitMs;
        _dahMs     = 3 * _unitMs;

        // Farnsworth gap formula (ARRL/ITU standard).
        // Standard inter-char gap at charWpm:
        uint32_t stdCharGap = 3 * _unitMs;
        // Total time for one PARIS word at charWpm element speed:
        //   50 units of mark/intra time, 31 are element time
        // Stretch gaps so effective rate = effectiveWpm.
        // wordMs = 60000 / effectiveWpm  (one PARIS word per minute)
        // element time for 50-unit PARIS = 31 * unitMs
        // remaining = wordMs - 31 * unitMs, split 4 charGaps + 1 wordGap (4+7=... )
        // Simplified standard formula:
        uint32_t stretchMs = (60000 / _effectiveWpm) - (31 * _unitMs);
        // 19 gap units total in PARIS (4 char-gaps * 3 + 1 word-gap * 7 = 19...
        // Actually standard breakdown: 4*inter-char(3) + 1*inter-word(7) = 19 units
        // But we're computing charGap and wordGap separately:
        // charGapMs = max(standard, stretched)
        // Stretched charGapMs via standard Farnsworth formula:
        uint32_t farnsworthCharGap = stretchMs * 3 / 19;  // 3 of 19 gap units
        _charGapMs = max(stdCharGap, farnsworthCharGap);
        // Word gap maintains 7:3 ratio to char gap.
        _wordGapMs = _charGapMs * 7 / 3;
    }
};
