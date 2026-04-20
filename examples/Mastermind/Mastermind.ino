/*
MIT License

Copyright (c) 2022-2025 jffordem

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

// Mastermind for Arduino UNO R4 Minima
// Hardware: LK204-25 20x4 LCD + 16-key keypad + 2 encoder wheels
//
// The computer picks a secret 4-color code from 6 colors (R G B Y W P).
// You have 10 guesses. After each guess you get clues:
//   * = right color, right position (black peg)
//   o = right color, wrong position (white peg)
//
// Controls:
//   Encoder Left  (wheel) : cycle the selected peg position (1-2-3-4)
//   Encoder Left  (click) : advance to next peg position
//   Encoder Right (wheel) : change the color at the selected position
//   Encoder Right (click) : submit guess  /  new game when game over
//   Key A                 : submit guess  /  new game when game over
//   Key B                 : clear current guess
//   Key *                 : new game (any time)
//
// Display layout during play:
//   Row 0: "MASTERMIND  G:02/10 "
//   Row 1: "#01 R G B Y  2*1o   "  <- older guess
//   Row 2: "#02 R G B Y  3*0o   "  <- most recent guess
//   Row 3: "> R _ _ _  A=Sub    "  <- current guess (cursor blinks on selected peg)

#include <Wire.h>
#include <Scheduler.hpp>
#include <Display.hpp>
#include <KeypadHandler.hpp>
#include <LK204_25.hpp>
#include <EncoderWheel.hpp>
#include <MinimaConfig.hpp>

// ─── Game constants ───────────────────────────────────────────────────────────

static const int  CODE_LEN    = 4;
static const int  MAX_GUESSES = 10;
static const int  NUM_COLORS  = 6;
static const char COLORS[]    = "RGBYWP";  // Red Green Blue Yellow White Purple

// ─── Game state ───────────────────────────────────────────────────────────────

enum GameState { MM_ATTRACT, MM_PLAYING, MM_WON, MM_LOST };

struct MastermindGame {
    int       code[CODE_LEN];
    int       guesses[MAX_GUESSES][CODE_LEN];
    int       blacks[MAX_GUESSES];
    int       whites[MAX_GUESSES];
    int       numGuesses;
    int       current[CODE_LEN];   // -1 = peg not yet set
    int       selectedPos;
    GameState state;

    MastermindGame() : numGuesses(0), selectedPos(0), state(MM_ATTRACT) {
        clearGuess();
    }

    // Seed randomness from the player's reaction time, then start.
    void begin() {
        randomSeed(millis());
        for (int i = 0; i < CODE_LEN; i++)
            code[i] = random(0, NUM_COLORS);
        numGuesses  = 0;
        selectedPos = 0;
        state       = MM_PLAYING;
        clearGuess();
    }

    void clearGuess() {
        for (int i = 0; i < CODE_LEN; i++) current[i] = -1;
    }

    bool allFilled() const {
        for (int i = 0; i < CODE_LEN; i++)
            if (current[i] < 0) return false;
        return true;
    }

    bool submit() {
        if (!allFilled() || state != MM_PLAYING) return false;

        for (int i = 0; i < CODE_LEN; i++)
            guesses[numGuesses][i] = current[i];

        // Clue calculation: count exact matches (blacks), then
        // color-match-in-wrong-position (whites) using frequency counts
        // only for the unmatched pegs.
        int b = 0;
        int codeCnt[NUM_COLORS]  = {};
        int guessCnt[NUM_COLORS] = {};
        for (int i = 0; i < CODE_LEN; i++) {
            if (current[i] == code[i]) {
                b++;
            } else {
                codeCnt[code[i]]++;
                guessCnt[current[i]]++;
            }
        }
        int w = 0;
        for (int c = 0; c < NUM_COLORS; c++)
            w += min(codeCnt[c], guessCnt[c]);

        blacks[numGuesses] = b;
        whites[numGuesses] = w;
        numGuesses++;

        if (b == CODE_LEN) {
            state = MM_WON;
        } else if (numGuesses >= MAX_GUESSES) {
            state = MM_LOST;
        } else {
            clearGuess();
        }
        return true;
    }

    void nextPos()   { selectedPos = (selectedPos + 1) % CODE_LEN; }
    void prevPos()   { selectedPos = (selectedPos + CODE_LEN - 1) % CODE_LEN; }

    void nextColor() {
        int &c = current[selectedPos];
        c = (c < 0) ? 0 : (c + 1) % NUM_COLORS;
    }
    void prevColor() {
        int &c = current[selectedPos];
        c = (c < 0) ? NUM_COLORS - 1 : (c + NUM_COLORS - 1) % NUM_COLORS;
    }

    char colorChar(int idx) const {
        if (idx < 0 || idx >= NUM_COLORS) return '_';
        return COLORS[idx];
    }

    bool isPlaying()  const { return state == MM_PLAYING; }
    bool isAttract()  const { return state == MM_ATTRACT; }
};

MastermindGame game;

// ─── Display renderer ─────────────────────────────────────────────────────────

class MastermindRenderer : public DisplayDrawable<LK204_25_LCD, 4, 20> {
    MastermindGame &_g;

    // Write one completed-guess row. Leaves row blank if guessIdx is out of range.
    // Format: "#01 R G B Y  2*1o   " (20 chars)
    void writeGuessRow(DisplayBuffer<4,20> &buf, int row, int guessIdx) {
        if (guessIdx < 0 || guessIdx >= _g.numGuesses) return;
        const int *g = _g.guesses[guessIdx];
        char line[21];
        snprintf(line, sizeof(line), "#%02d %c %c %c %c  %d*%do   ",
            guessIdx + 1,
            _g.colorChar(g[0]), _g.colorChar(g[1]),
            _g.colorChar(g[2]), _g.colorChar(g[3]),
            _g.blacks[guessIdx], _g.whites[guessIdx]);
        buf.write(row, 0, line, 20);
    }

    // Write the current-guess input row.
    // Format: "> R G _ _  A=Sub    " (20 chars)
    // The LCD cursor blinks on the selected peg (handled via wantsCursor/cursorPosition).
    void writeCurrentRow(DisplayBuffer<4,20> &buf) {
        char line[21];
        snprintf(line, sizeof(line), "> %c %c %c %c  A=Sub    ",
            _g.colorChar(_g.current[0]), _g.colorChar(_g.current[1]),
            _g.colorChar(_g.current[2]), _g.colorChar(_g.current[3]));
        buf.write(3, 0, line, 20);
    }

public:
    MastermindRenderer(MastermindGame &g) : _g(g) { }

    bool wantsCursor() override { return _g.isPlaying(); }

    void cursorPosition(int &col, int &row) override {
        // Pegs are at cols 2, 4, 6, 8 on row 3
        row = 3;
        col = 2 + _g.selectedPos * 2;
    }

    void draw(DisplayBuffer<4,20> &buf) override {
        buf.clear(' ');

        if (_g.isAttract()) {
            buf.write(0, 0, "    MASTERMIND      ", 20);
            buf.write(1, 0, "  Crack the code!   ", 20);
            buf.write(2, 0, "6 colors, 10 guesses", 20);
            buf.write(3, 0, " Press any key...   ", 20);
            return;
        }

        if (_g.state == MM_WON) {
            char line[21];
            buf.write(0, 0, "** YOU WIN! **      ", 20);
            snprintf(line, sizeof(line), "Cracked it: %2d/%-2d   ",
                _g.numGuesses, MAX_GUESSES);
            buf.write(1, 0, line, 20);
            snprintf(line, sizeof(line), "Code: %c %c %c %c       ",
                COLORS[_g.code[0]], COLORS[_g.code[1]],
                COLORS[_g.code[2]], COLORS[_g.code[3]]);
            buf.write(2, 0, line, 20);
            buf.write(3, 0, "A or * = New Game   ", 20);
            return;
        }

        if (_g.state == MM_LOST) {
            char line[21];
            buf.write(0, 0, "** GAME OVER **     ", 20);
            buf.write(1, 0, "Better luck next... ", 20);
            snprintf(line, sizeof(line), "Code: %c %c %c %c       ",
                COLORS[_g.code[0]], COLORS[_g.code[1]],
                COLORS[_g.code[2]], COLORS[_g.code[3]]);
            buf.write(2, 0, line, 20);
            buf.write(3, 0, "A or * = New Game   ", 20);
            return;
        }

        // Playing: status + last 2 guesses + current input row
        char status[21];
        snprintf(status, sizeof(status), "MASTERMIND  G:%02d/%-2d ",
            _g.numGuesses + 1, MAX_GUESSES);
        buf.write(0, 0, status, 20);

        int n = _g.numGuesses;
        writeGuessRow(buf, 1, n - 2);   // blank if fewer than 2 guesses
        writeGuessRow(buf, 2, n - 1);   // blank if no guesses yet
        writeCurrentRow(buf);
    }
};

// ─── Encoder handlers ─────────────────────────────────────────────────────────

// Encoder A (Left): cycles the selected peg position.
class PosEncoder : public EncoderWheelHandler {
    MastermindGame &_g;
public:
    PosEncoder(Schedule &s, const EncoderConfig &cfg, MastermindGame &g) :
        EncoderWheelHandler(s, cfg), _g(g) { }
    void handleInput(uint8_t input) override {
        if (!_g.isPlaying()) return;
        if (input == ENCODER_WHEEL_RIGHT) _g.nextPos();
        else                              _g.prevPos();
    }
};

// Encoder B (Right): changes the color at the currently selected position.
class ColorEncoder : public EncoderWheelHandler {
    MastermindGame &_g;
public:
    ColorEncoder(Schedule &s, const EncoderConfig &cfg, MastermindGame &g) :
        EncoderWheelHandler(s, cfg), _g(g) { }
    void handleInput(uint8_t input) override {
        if (!_g.isPlaying()) return;
        if (input == ENCODER_WHEEL_RIGHT) _g.nextColor();
        else                              _g.prevColor();
    }
};

// ─── Encoder button callbacks ─────────────────────────────────────────────────
// Free functions required because ButtonHandler takes void(*)() function pointers.

static void onLeftButtonPress() {
    // Click left knob to step forward through peg positions.
    if (game.isPlaying()) game.nextPos();
    else if (game.isAttract()) game.begin();
}

static void onRightButtonPress() {
    // Click right knob to submit the current guess (or start / restart).
    if (game.isPlaying()) game.submit();
    else                  game.begin();
}

// ─── Keypad handler ───────────────────────────────────────────────────────────

class MastermindKeys : public KeypadKeyHandler {
    MastermindGame &_g;
public:
    MastermindKeys(MastermindGame &g) : _g(g) { }

    bool handle_key(char ch) override {
        // Any key in attract mode seeds randomness from human reaction time
        // and starts the game — this is the main randomisation trick.
        if (_g.isAttract()) {
            _g.begin();
            return true;
        }

        if (ch == KeypadKey_A) {
            if (_g.isPlaying()) _g.submit();
            else                _g.begin();   // new game after win/loss
            return true;
        }
        if (ch == KeypadKey_B) {
            if (_g.isPlaying()) _g.clearGuess();
            return true;
        }
        if (ch == KeypadKey_Asterisk) {
            _g.begin();
            return true;
        }
        return false;
    }
};

// ─── Hardware wiring ──────────────────────────────────────────────────────────

MainSchedule    schedule;
LK204_25_LCD    lcd;
LK204_25_Keypad keypad;

MastermindRenderer renderer(game);
MainDisplay<LK204_25_LCD, 4, 20> display(schedule, lcd, renderer, 100, 5000L);

MastermindKeys keys(game);
KeypadHandler  keypadHandler(schedule, keypad, keys);

PosEncoder   posEnc  (schedule, Config.Left.Encoder,  game);
ColorEncoder colorEnc(schedule, Config.Right.Encoder, game);

ButtonHandler leftEncBtn (schedule, Config.Left.Button,  onLeftButtonPress);
ButtonHandler rightEncBtn(schedule, Config.Right.Button, onRightButtonPress);

// ─── Sketch entry points ──────────────────────────────────────────────────────

void setup() {
    keypad.begin();
    display.begin();
    schedule.begin();
    // Game starts in MM_ATTRACT; first keypress seeds random and starts play.
}

void loop() {
    schedule.poll();
}
