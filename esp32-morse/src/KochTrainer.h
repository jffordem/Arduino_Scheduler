#pragma once
#include <Arduino.h>
#include <esp_random.h>
#include "MorseEngine.h"
#include "Timer.h"

class KochTrainer {
public:
    static constexpr int MIN_LEVEL = 2;
    static constexpr int MAX_LEVEL = 40;

    // ITU Koch order — 40 chars. Mirror this string in index.html JS.
    static const char* kochOrder() {
        return "KMRSUAPTLOWI.NJEF0YV,G5/Q9ZH38B?427C1D6X";
    }

    KochTrainer(MorseEngine& engine)
        : _engine(engine), _state(IDLE), _kochLevel(MIN_LEVEL),
          _correct(0), _total(0), _lastSent('\0'),
          _revealed(false), _lastCorrect(false)
    {
        _lastAnswer[0] = '\0';
    }

    void setLevel(int level)      { _kochLevel = constrain(level, MIN_LEVEL, MAX_LEVEL); }
    void setCharWpm(int wpm)      { _engine.setCharWpm(wpm); }
    void setEffectiveWpm(int wpm) { _engine.setEffectiveWpm(wpm); }
    int  kochLevel() const        { return _kochLevel; }
    bool running()   const        { return _state != IDLE; }

    void start() {
        _correct = _total = 0;
        _lastAnswer[0] = '\0';
        randomSeed(esp_random());
        _pickAndSend();
    }

    void stop() {
        _engine.stop();
        _state = IDLE;
    }

    // Accepts answers in WAITING (normal) or SENDING (early — stops engine).
    void submitAnswer(const char* input) {
        if (_state != WAITING && _state != SENDING) return;
        if (_state == SENDING) _engine.stop();

        const char* p = input;
        while (*p == ' ') p++;
        _lastAnswer[0] = *p ? (char)toupper((unsigned char)*p) : '\0';
        _lastAnswer[1] = '\0';

        _lastCorrect = (_lastAnswer[0] && _lastAnswer[0] == _lastSent);
        if (_lastCorrect) _correct++;
        _total++;
        _revealed = true;
        _interTimer.reset(2000);
        _state = INTER;
    }

    void poll(uint32_t /*now*/) {
        switch (_state) {
            case SENDING:
                if (!_engine.sending()) {
                    _revealTimer.reset(5000);
                    _state = WAITING;
                }
                break;
            case WAITING:
                if (_revealTimer.expired()) {
                    // Auto-reveal — count as miss.
                    _lastAnswer[0] = '\0';
                    _lastCorrect   = false;
                    _total++;
                    _revealed = true;
                    _interTimer.reset(2000);
                    _state = INTER;
                }
                break;
            case INTER:
                if (_interTimer.expired()) _pickAndSend();
                break;
            case IDLE:
                break;
        }
    }

    String statusJson() const {
        char lastSentJson[6]   = "\"\"";
        char lastAnswerJson[6] = "\"\"";
        if (_revealed && _lastSent)
            snprintf(lastSentJson,   sizeof(lastSentJson),   "\"%c\"", _lastSent);
        if (_revealed && _lastAnswer[0])
            snprintf(lastAnswerJson, sizeof(lastAnswerJson), "\"%c\"", _lastAnswer[0]);

        int pct = _total > 0 ? (_correct * 100 / _total) : 0;
        char buf[320];
        snprintf(buf, sizeof(buf),
            "{\"running\":%s,\"kochLevel\":%d,\"charWpm\":%d,\"effectiveWpm\":%d,"
            "\"lastSent\":%s,\"revealed\":%s,\"lastAnswer\":%s,\"lastCorrect\":%s,"
            "\"correct\":%d,\"total\":%d,\"pct\":%d}",
            running()    ? "true" : "false",
            _kochLevel,
            _engine.charWpm(),
            _engine.effectiveWpm(),
            lastSentJson,
            _revealed    ? "true" : "false",
            lastAnswerJson,
            _lastCorrect ? "true" : "false",
            _correct, _total, pct);
        return String(buf);
    }

private:
    enum State { IDLE, SENDING, WAITING, INTER };

    MorseEngine& _engine;
    State        _state;
    int          _kochLevel;
    int          _correct, _total;
    char         _lastSent;
    bool         _revealed, _lastCorrect;
    char         _lastAnswer[4];

    Timer        _revealTimer;
    Timer        _interTimer;

    void _pickAndSend() {
        int idx = random(0, _kochLevel);
        _lastSent      = kochOrder()[idx];
        _revealed      = false;
        _lastCorrect   = false;
        _lastAnswer[0] = '\0';
        char msg[2]    = { _lastSent, '\0' };
        _engine.send(msg);
        _state = SENDING;
    }
};
