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

// MicroZork — text adventure for Arduino UNO R4 Minima
// Hardware: LK204-25 20x4 LCD + 16-key keypad + 2 encoder wheels
//
// Story: The crypt holds ancient treasure. Find it and bring it
// back to the Forest Path to win.  Three obstacles stand in the way:
//   1. The crypt door is locked — find the Iron Key on the ledge.
//   2. The crypt depths are pitch dark — find and light the Brass Lamp.
//   3. A skeleton guards the vault — your Sword can deal with that.
//
// Controls:
//   Encoder Left  (wheel) : scroll description up / down
//   Encoder Left  (click) : Look (refresh room description)
//   Encoder Right (wheel) : cycle available actions
//   Encoder Right (click) : execute selected action
//   Key A                 : execute selected action
//   Key B                 : Look (refresh room description)
//   Keys 1-9              : execute action #N directly
//   Key *                 : restart game (any time)
//
// Display:
//   Row 0: "Forest Path      [L]"  <- room name (17) + lamp indicator (3)
//   Row 1: "A mossy path winds ^"  <- desc line, ^ = can scroll up
//   Row 2: "through dark pines v"  <- desc line, v = can scroll down
//   Row 3: "<1:Go East          >" <- action bar (< prev  N:label  > next)

#include <Wire.h>
#include <Scheduler.hpp>
#include <Display.hpp>
#include <KeypadHandler.hpp>
#include <LK204_25.hpp>
#include <EncoderWheel.hpp>
#include <MinimaConfig.hpp>

// ─── World constants ──────────────────────────────────────────────────────────

static const int NUM_ROOMS      = 7;
static const int NUM_ITEMS      = 4;
static const int MAX_ACTIONS    = 14;
static const int MAX_DESC_LINES = 10;
static const int COL            = 20;

// Item IDs
static const int ITEM_SWORD    = 0;
static const int ITEM_KEY      = 1;
static const int ITEM_LAMP     = 2;
static const int ITEM_TREASURE = 3;

// param value for "Douse Lamp" action — must not collide with item IDs
static const int PARAM_DOUSE   = ITEM_LAMP + 100;

// Room IDs
static const int ROOM_FOREST  = 0;
static const int ROOM_CLEARING = 1;
static const int ROOM_CRYPT   = 2;
static const int ROOM_LEDGE   = 3;
static const int ROOM_ANTE    = 4;
static const int ROOM_DEPTHS  = 5;
static const int ROOM_VAULT   = 6;

// ─── World data ───────────────────────────────────────────────────────────────

static const char * const ROOM_NAMES[NUM_ROOMS] = {
    "Forest Path",
    "Sunlit Clearing",
    "Crypt Entrance",
    "Craggy Ledge",
    "Antechamber",
    "Crypt Depths",
    "Treasure Vault",
};

// Two static description lines per room (each ≤ 19 chars — last char is scroll indicator)
static const char * const ROOM_DESC[NUM_ROOMS][2] = {
    { "A mossy path winds", "through dark pines." },
    { "Sunlight floods a",  "grassy clearing."    },
    { "Stone steps descend","into cold darkness." },
    { "A narrow ledge hugs","the cliff face."     },
    { "Dusty shelves line", "the stone walls."    },
    { "Total blackness.",   "Something breathes.." },
    { "Ancient riches fill","the vaulted chamber."},
};

static const char * const ITEM_NAMES[NUM_ITEMS] = {
    "Sword", "Iron Key", "Brass Lamp", "Treasure"
};

// ─── Game state ───────────────────────────────────────────────────────────────

// Action types
static const int ACT_GO   = 0;  // param = dest room id
static const int ACT_TAKE = 1;  // param = item id
static const int ACT_DROP = 2;  // param = item id
static const int ACT_USE  = 3;  // param = item id (or PARAM_DOUSE)
static const int ACT_LOOK = 4;  // param = -1 → show locked message, else look

struct ZAction {
    char label[19];  // 18 chars + null
    int  type;
    int  param;
};

enum ZState { ZK_ATTRACT, ZK_PLAYING, ZK_WON };

struct ZorkGame {
    int     room;
    bool    hasItem[NUM_ITEMS];
    bool    inRoom[NUM_ROOMS][NUM_ITEMS];
    bool    lampLit;
    bool    cryptLocked;
    bool    skeletonAlive;
    ZState  state;

    char    descBuf[MAX_DESC_LINES][COL + 1];
    int     numDescLines;
    int     descScroll;

    ZAction actions[MAX_ACTIONS];
    int     numActions;
    int     selectedAction;

    char         msgBuf[COL + 1];
    bool         hasMsg;
    unsigned long msgExpiry;

    ZorkGame() : room(ROOM_FOREST), lampLit(false), cryptLocked(true),
                 skeletonAlive(true), state(ZK_ATTRACT),
                 numDescLines(0), descScroll(0), numActions(0), selectedAction(0),
                 hasMsg(false), msgExpiry(0) {
        memset(hasItem, 0, sizeof(hasItem));
        memset(inRoom,  0, sizeof(inRoom));
        placeItems();
    }

    void placeItems() {
        inRoom[ROOM_CLEARING][ITEM_SWORD]   = true;
        inRoom[ROOM_LEDGE][ITEM_KEY]        = true;
        inRoom[ROOM_ANTE][ITEM_LAMP]        = true;
        inRoom[ROOM_VAULT][ITEM_TREASURE]   = true;
    }

    void begin() {
        room          = ROOM_FOREST;
        lampLit       = false;
        cryptLocked   = true;
        skeletonAlive = true;
        state         = ZK_PLAYING;
        memset(hasItem, 0, sizeof(hasItem));
        memset(inRoom,  0, sizeof(inRoom));
        placeItems();
        look();
    }

    bool isPlaying() const { return state == ZK_PLAYING; }
    bool isAttract() const { return state == ZK_ATTRACT; }
    bool isDark()    const { return room == ROOM_DEPTHS && !lampLit; }

    void showMsg(const char *msg, unsigned long durationMs = 2500) {
        strncpy(msgBuf, msg, COL);
        msgBuf[COL] = '\0';
        hasMsg    = true;
        msgExpiry = millis() + durationMs;
    }

    void pollMsg() {
        if (hasMsg && millis() >= msgExpiry) hasMsg = false;
    }

    void addDesc(const char *text) {
        if (numDescLines >= MAX_DESC_LINES) return;
        strncpy(descBuf[numDescLines], text, COL);
        descBuf[numDescLines][COL] = '\0';
        numDescLines++;
    }

    void look() {
        numDescLines = 0;
        descScroll   = 0;
        hasMsg       = false;

        if (isDark()) {
            addDesc("It is pitch dark.");
            addDesc("You may be eaten");
            addDesc("by a grue.");
            buildActions();
            return;
        }

        addDesc(ROOM_DESC[room][0]);
        addDesc(ROOM_DESC[room][1]);

        for (int i = 0; i < NUM_ITEMS; i++) {
            if (inRoom[room][i]) {
                char line[COL + 1];
                snprintf(line, sizeof(line), "You see: %s", ITEM_NAMES[i]);
                addDesc(line);
            }
        }

        if (room == ROOM_DEPTHS && skeletonAlive) {
            addDesc("A skeleton blocks");
            addDesc("the east passage.");
        }

        bool carrying = false;
        for (int i = 0; i < NUM_ITEMS; i++) {
            if (hasItem[i]) {
                if (!carrying) { addDesc("Carrying:"); carrying = true; }
                char line[COL + 1];
                snprintf(line, sizeof(line), "  %s", ITEM_NAMES[i]);
                addDesc(line);
            }
        }

        buildActions();
    }

    void addAction(const char *label, int type, int param = 0) {
        if (numActions >= MAX_ACTIONS) return;
        strncpy(actions[numActions].label, label, 18);
        actions[numActions].label[18] = '\0';
        actions[numActions].type  = type;
        actions[numActions].param = param;
        numActions++;
    }

    void buildActions() {
        numActions      = 0;
        selectedAction  = 0;

        // Exits — defined per room
        switch (room) {
            case ROOM_FOREST:
                addAction("Go East",  ACT_GO, ROOM_CLEARING);
                addAction("Go North", ACT_GO, ROOM_CRYPT);
                break;
            case ROOM_CLEARING:
                addAction("Go West",  ACT_GO, ROOM_FOREST);
                break;
            case ROOM_CRYPT:
                addAction("Go South", ACT_GO, ROOM_FOREST);
                addAction("Go East",  ACT_GO, ROOM_LEDGE);
                if (!cryptLocked) addAction("Go North", ACT_GO, ROOM_ANTE);
                else              addAction("N: Door Locked", ACT_LOOK, -1);
                break;
            case ROOM_LEDGE:
                addAction("Go West",  ACT_GO, ROOM_CRYPT);
                break;
            case ROOM_ANTE:
                addAction("Go South", ACT_GO, ROOM_CRYPT);
                addAction("Go North", ACT_GO, ROOM_DEPTHS);
                break;
            case ROOM_DEPTHS:
                addAction("Go South", ACT_GO, ROOM_ANTE);
                if (!skeletonAlive) addAction("Go East", ACT_GO, ROOM_VAULT);
                break;
            case ROOM_VAULT:
                addAction("Go West",  ACT_GO, ROOM_DEPTHS);
                break;
        }

        // Take items in this room
        for (int i = 0; i < NUM_ITEMS; i++) {
            if (inRoom[room][i]) {
                char lbl[19];
                snprintf(lbl, sizeof(lbl), "Take %s", ITEM_NAMES[i]);
                addAction(lbl, ACT_TAKE, i);
            }
        }

        // Drop items in inventory
        for (int i = 0; i < NUM_ITEMS; i++) {
            if (hasItem[i]) {
                char lbl[19];
                snprintf(lbl, sizeof(lbl), "Drop %s", ITEM_NAMES[i]);
                addAction(lbl, ACT_DROP, i);
            }
        }

        // Contextual use actions
        if (hasItem[ITEM_KEY] && room == ROOM_CRYPT && cryptLocked)
            addAction("Use Iron Key", ACT_USE, ITEM_KEY);

        if (hasItem[ITEM_LAMP] && !lampLit)
            addAction("Light Lamp",   ACT_USE, ITEM_LAMP);

        if (hasItem[ITEM_LAMP] && lampLit)
            addAction("Douse Lamp",   ACT_USE, PARAM_DOUSE);

        if (hasItem[ITEM_SWORD] && room == ROOM_DEPTHS && skeletonAlive)
            addAction("Attack Skeleton", ACT_USE, ITEM_SWORD);

        addAction("Look", ACT_LOOK);
    }

    void execute(int idx) {
        if (idx < 0 || idx >= numActions || state != ZK_PLAYING) return;
        ZAction &a = actions[idx];

        switch (a.type) {
            case ACT_GO:
                room = a.param;
                look();
                if (room == ROOM_FOREST && hasItem[ITEM_TREASURE]) state = ZK_WON;
                break;

            case ACT_TAKE:
                inRoom[room][a.param] = false;
                hasItem[a.param]      = true;
                {
                    char msg[COL + 1];
                    snprintf(msg, sizeof(msg), "Taken: %s", ITEM_NAMES[a.param]);
                    showMsg(msg);
                }
                look();
                if (a.param == ITEM_TREASURE && room == ROOM_FOREST) state = ZK_WON;
                break;

            case ACT_DROP:
                hasItem[a.param]      = false;
                inRoom[room][a.param] = true;
                {
                    char msg[COL + 1];
                    snprintf(msg, sizeof(msg), "Dropped: %s", ITEM_NAMES[a.param]);
                    showMsg(msg);
                }
                look();
                break;

            case ACT_USE:
                if (a.param == ITEM_KEY) {
                    cryptLocked = false;
                    showMsg("The door grinds open!");
                    look();
                } else if (a.param == ITEM_LAMP) {
                    lampLit = true;
                    showMsg("The lamp flickers on.");
                    look();
                } else if (a.param == PARAM_DOUSE) {
                    lampLit = false;
                    showMsg("Darkness falls.");
                    look();
                } else if (a.param == ITEM_SWORD) {
                    skeletonAlive = false;
                    showMsg("The skeleton falls!");
                    look();
                }
                break;

            case ACT_LOOK:
                if (a.param == -1) showMsg("The door is locked.");
                else               look();
                break;
        }
    }

    void executeSelected() { execute(selectedAction); }

    void nextAction() {
        if (numActions > 0) selectedAction = (selectedAction + 1) % numActions;
    }
    void prevAction() {
        if (numActions > 0) selectedAction = (selectedAction + numActions - 1) % numActions;
    }

    void scrollUp()   { if (descScroll > 0) descScroll--; }
    void scrollDown() { if (descScroll + 2 < numDescLines) descScroll++; }
};

ZorkGame game;

// ─── Renderer ─────────────────────────────────────────────────────────────────

class ZorkRenderer : public DisplayDrawable<LK204_25_LCD, 4, 20> {
    ZorkGame &_g;
public:
    ZorkRenderer(ZorkGame &g) : _g(g) { }

    bool wantsCursor() override { return false; }
    void cursorPosition(int &col, int &row) override { col = 0; row = 3; }

    void draw(DisplayBuffer<4, 20> &buf) override {
        _g.pollMsg();
        buf.clear(' ');

        if (_g.isAttract()) {
            buf.write(0, 0, "   ** MICROZORK **  ", 20);
            buf.write(1, 0, " A text adventure   ", 20);
            buf.write(2, 0, " in 20x4 characters ", 20);
            buf.write(3, 0, "   Press any key... ", 20);
            return;
        }

        if (_g.state == ZK_WON) {
            buf.write(0, 0, "** YOU WIN! **      ", 20);
            buf.write(1, 0, "Treasure returned to", 20);
            buf.write(2, 0, "Forest Path. Huzzah!", 20);
            buf.write(3, 0, "* = New Game        ", 20);
            return;
        }

        // Row 0: room name (17 chars) + lamp indicator (3 chars)
        {
            char row0[21];
            snprintf(row0, sizeof(row0), "%-17.17s%s",
                ROOM_NAMES[_g.room], _g.lampLit ? "[L]" : "   ");
            buf.write(0, 0, row0, 20);
        }

        // Rows 1-2: scrollable description (2 visible lines at a time)
        for (int row = 1; row <= 2; row++) {
            int lineIdx = _g.descScroll + (row - 1);
            char scrollCh = ' ';
            if (row == 1 && _g.descScroll > 0)                       scrollCh = '^';
            if (row == 2 && _g.descScroll + 2 < _g.numDescLines)    scrollCh = 'v';

            // Timed message replaces row 2 content
            if (_g.hasMsg && row == 2) {
                char msgRow[21];
                snprintf(msgRow, sizeof(msgRow), "%-19.19s%c", _g.msgBuf, scrollCh);
                buf.write(row, 0, msgRow, 20);
            } else if (lineIdx >= 0 && lineIdx < _g.numDescLines) {
                char descRow[21];
                snprintf(descRow, sizeof(descRow), "%-19.19s%c",
                    _g.descBuf[lineIdx], scrollCh);
                buf.write(row, 0, descRow, 20);
            }
        }

        // Row 3: action bar  "<N:label          >"
        // N = 1-based index digit (blank if action index ≥ 9)
        if (_g.numActions > 0) {
            char numStr[3];
            if (_g.selectedAction <= 8) {
                numStr[0] = '1' + _g.selectedAction;
                numStr[1] = ':';
                numStr[2] = '\0';
            } else {
                strcpy(numStr, "  ");
            }
            const char *lbl  = _g.actions[_g.selectedAction].label;
            bool        many = _g.numActions > 1;
            char actRow[21];
            snprintf(actRow, sizeof(actRow), "%c%s%-16.16s%c",
                many ? '<' : ' ', numStr, lbl, many ? '>' : ' ');
            buf.write(3, 0, actRow, 20);
        }
    }
};

// ─── Encoder handlers ─────────────────────────────────────────────────────────

class DescEncoder : public EncoderWheelHandler {
    ZorkGame &_g;
public:
    DescEncoder(Schedule &s, const EncoderConfig &cfg, ZorkGame &g) :
        EncoderWheelHandler(s, cfg), _g(g) { }
    void handleInput(uint8_t input) override {
        if (!_g.isPlaying()) return;
        if (input == ENCODER_WHEEL_RIGHT) _g.scrollDown();
        else                              _g.scrollUp();
    }
};

class ActionEncoder : public EncoderWheelHandler {
    ZorkGame &_g;
public:
    ActionEncoder(Schedule &s, const EncoderConfig &cfg, ZorkGame &g) :
        EncoderWheelHandler(s, cfg), _g(g) { }
    void handleInput(uint8_t input) override {
        if (!_g.isPlaying()) return;
        if (input == ENCODER_WHEEL_RIGHT) _g.nextAction();
        else                              _g.prevAction();
    }
};

// ─── Encoder button callbacks ─────────────────────────────────────────────────

static void onLeftClick() {
    if (game.isAttract()) game.begin();
    else if (game.isPlaying()) game.look();
}

static void onRightClick() {
    if (game.isAttract()) game.begin();
    else if (game.isPlaying()) game.executeSelected();
}

// ─── Keypad handler ───────────────────────────────────────────────────────────

class ZorkKeys : public KeypadKeyHandler {
    ZorkGame &_g;
public:
    ZorkKeys(ZorkGame &g) : _g(g) { }

    bool handle_key(char ch) override {
        if (_g.isAttract()) { _g.begin(); return true; }

        if (ch == KeypadKey_Asterisk) { _g.begin(); return true; }

        if (!_g.isPlaying()) return false;

        if (ch == KeypadKey_A) { _g.executeSelected(); return true; }
        if (ch == KeypadKey_B) { _g.look();            return true; }

        // Digit 1-9 executes action by position
        if (ch >= '1' && ch <= '9') {
            _g.execute(ch - '1');
            return true;
        }

        return false;
    }
};

// ─── Hardware setup ───────────────────────────────────────────────────────────

MainSchedule    schedule;
LK204_25_LCD    lcd;
LK204_25_Keypad keypad;

ZorkRenderer renderer(game);
MainDisplay<LK204_25_LCD, 4, 20> display(schedule, lcd, renderer, 100, 5000L);

ZorkKeys     keys(game);
KeypadHandler keypadHandler(schedule, keypad, keys);

DescEncoder   descEnc  (schedule, Config.Left.Encoder,  game);
ActionEncoder actionEnc(schedule, Config.Right.Encoder, game);

ButtonHandler leftEncBtn (schedule, Config.Left.Button,  onLeftClick);
ButtonHandler rightEncBtn(schedule, Config.Right.Button, onRightClick);

// ─── Sketch entry points ──────────────────────────────────────────────────────

void setup() {
    keypad.begin();
    display.begin();
    schedule.begin();
    // Game starts in ZK_ATTRACT; first keypress starts play.
}

void loop() {
    schedule.poll();
}
