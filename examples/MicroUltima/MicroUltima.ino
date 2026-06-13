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

// MicroUltima — tile-based RPG for Arduino UNO R4 Minima
// Hardware: LK204-25 20x4 LCD + 16-key keypad + 2 encoder wheels
//
// Explore a 32x16 world, fight monsters, level up, and defeat all foes.
//
// Keypad layout mirrors the classic numpad:
//   [7][8][9]      NW  N  NE
//   [4][@][6]   =   W  .   E
//   [1][2][3]      SW  S  SE
//   [5] = Wait
//   [A] = Attack / confirm
//   [B] = Flee / Leave town
//   [1]-[4] in towns = buy/upgrade
//   [*] = New Game
//
// Map tiles:
//   .  plains (passable)   ^  mountain (impassable)
//   ~  water  (impassable) T  forest   (passable)
//   t  Town 1              u  Town 2
//   C  Castle (landmark)   D  Dungeon (future)
//
// Enemies on map:
//   g Goblin  o Orc  R Troll  X Dragon (final boss)

#include <Wire.h>
#include <Scheduler.hpp>
#include <Display.hpp>
#include <KeypadHandler.hpp>
#include <LK204_25.hpp>
#include <EncoderWheel.hpp>
#include <MinimaConfig.hpp>

// ─── World ────────────────────────────────────────────────────────────────────

static const int MAP_W = 32;
static const int MAP_H = 16;
static const int VW    = 20;   // viewport width = full display width
static const int VH    = 3;    // map rows shown

static char world[MAP_H][MAP_W];

static void   setTile(int x, int y, char ch) {
    if (x >= 0 && x < MAP_W && y >= 0 && y < MAP_H) world[y][x] = ch;
}
static char   getTile(int x, int y) {
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return '^';
    return world[y][x];
}
static bool   passable(char tile) { return tile != '^' && tile != '~'; }

static void buildMap() {
    // Fill with plains
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            world[y][x] = '.';

    // Mountain border (4 tiles on sides, 1 on top/bottom)
    for (int x = 0; x < MAP_W; x++) {
        setTile(x, 0, '^');
        setTile(x, MAP_H - 1, '^');
    }
    for (int y = 0; y < MAP_H; y++) {
        for (int b = 0; b < 4; b++) {
            setTile(b, y, '^');
            setTile(MAP_W - 1 - b, y, '^');
        }
    }

    // Interior mountain ridge (east section) with a gap at y=7
    for (int y = 2; y < MAP_H - 2; y++) {
        setTile(19, y, '^');
        setTile(20, y, '^');
    }
    setTile(19, 7, '.');
    setTile(20, 7, '.');

    // Water lake (south-center)
    for (int y = 10; y <= 12; y++)
        for (int x = 8; x <= 14; x++)
            setTile(x, y, '~');

    // Forest scatter
    static const int trees[][2] = {
        {6,2},{8,2},{10,2},{7,3},{9,3},{11,3},
        {5,5},{7,5},{9,5},{6,6},{8,6},{10,6},
        {5,8},{7,8},{6,10},{7,11},
        {22,3},{24,3},{23,4},{25,5},{22,6},{24,7},
        {23,9},{25,10},{22,11},{24,12}
    };
    for (int i = 0; i < (int)(sizeof(trees)/sizeof(trees[0])); i++)
        setTile(trees[i][0], trees[i][1], 'T');

    // Special features
    setTile(13, 2, 't');  // Town 1: Brightkeep (NE quadrant)
    setTile(9,  13, 'u'); // Town 2: Shadowfen  (SW quadrant)
    setTile(5,  8,  'C'); // Castle (western landmark)
    setTile(14, 6,  'D'); // Dungeon entrance (center)
}

// ─── Game data ────────────────────────────────────────────────────────────────

struct EnemyType {
    const char *name;
    char        glyph;
    int         maxHp, atk, def;
    int         xpReward, gpReward;
};

static const EnemyType ETYPES[] = {
    { "Goblin", 'g',  6, 2, 1,  3,  2 },
    { "Orc",    'o', 10, 3, 2,  7,  5 },
    { "Troll",  'R', 16, 4, 3, 14, 10 },
    { "Dragon", 'X', 28, 6, 4, 30, 20 },
};
static const int NUM_ETYPES = 4;

// XP needed to *reach* each level (index = target level)
static const int LEVEL_XP[] = { 0, 0, 10, 25, 45, 70, 100 };
static const int MAX_LEVEL  = 6;

// ─── Game state ───────────────────────────────────────────────────────────────

struct Enemy {
    int  x, y, type, hp;
    bool alive;
};

static const int MAX_ENEMIES = 8;

struct UltimaGame {
    enum Phase { ATTRACT, OVERWORLD, COMBAT, TOWN, GAMEOVER, WIN } phase;

    // Player
    int px, py;
    int hp, maxHp, gp, xp, level, atk, def;

    // Enemies
    Enemy enemies[MAX_ENEMIES];
    int   numEnemies;
    int   fightIdx;         // -1 when not in combat

    // Message lines
    char  combatMsg[21];    // updated each combat round
    char  eventMsg[21];     // timed overlay on overworld / town
    bool  hasEventMsg;
    unsigned long eventExpiry;

    // Town
    int townId;

    UltimaGame() : phase(ATTRACT) {}

    void begin() {
        buildMap();

        px = 10;  py = 8;
        hp = 15;  maxHp = 15;
        gp = 20;  xp = 0;  level = 1;
        atk = 3;  def = 2;
        fightIdx   = -1;
        townId     = -1;
        hasEventMsg = false;
        phase      = OVERWORLD;
        strcpy(combatMsg, "");

        // Enemy placement: {x, y, type}
        static const int ep[][3] = {
            {  8,  3, 0 },   // Goblin NW
            { 11,  5, 0 },   // Goblin center
            { 15,  3, 1 },   // Orc near town1
            { 12,  8, 1 },   // Orc center
            {  6, 11, 0 },   // Goblin SW
            { 16, 10, 2 },   // Troll east of lake
            { 22,  5, 1 },   // Orc across ridge
            { 24,  9, 3 },   // Dragon (far east, final boss)
        };
        numEnemies = (int)(sizeof(ep) / sizeof(ep[0]));
        for (int i = 0; i < numEnemies; i++) {
            enemies[i] = { ep[i][0], ep[i][1], ep[i][2],
                           ETYPES[ep[i][2]].maxHp, true };
        }
    }

    void showEvent(const char *msg, unsigned long ms = 2500) {
        strncpy(eventMsg, msg, 20);
        eventMsg[20]  = '\0';
        hasEventMsg   = true;
        eventExpiry   = millis() + ms;
    }

    void pollEvent() {
        if (hasEventMsg && millis() >= eventExpiry) hasEventMsg = false;
    }

    char displayTile(int mx, int my) const {
        for (int i = 0; i < numEnemies; i++)
            if (enemies[i].alive && enemies[i].x == mx && enemies[i].y == my)
                return ETYPES[enemies[i].type].glyph;
        return getTile(mx, my);
    }

    void tryMove(int dx, int dy) {
        int nx = px + dx, ny = py + dy;

        // Enemy encounter?
        for (int i = 0; i < numEnemies; i++) {
            if (enemies[i].alive && enemies[i].x == nx && enemies[i].y == ny) {
                fightIdx = i;
                strcpy(combatMsg, "");
                phase = COMBAT;
                return;
            }
        }

        if (!passable(getTile(nx, ny))) return;
        px = nx;  py = ny;

        char tile = getTile(px, py);
        if (tile == 't') { townId = 0; phase = TOWN; }
        else if (tile == 'u') { townId = 1; phase = TOWN; }
        else if (tile == 'D') showEvent("Dungeon sealed.");
        else if (tile == 'C') showEvent("The castle looms.");
    }

    void attackEnemy() {
        if (fightIdx < 0 || phase != COMBAT) return;
        Enemy &e        = enemies[fightIdx];
        const EnemyType &et = ETYPES[e.type];

        int dmg = max(1, atk + (int)random(0, 3) - et.def);
        e.hp -= dmg;

        if (e.hp <= 0) {
            e.alive = false;
            xp += et.xpReward;
            gp += et.gpReward;
            snprintf(combatMsg, sizeof(combatMsg), "Win!+%dXP +%dGP", et.xpReward, et.gpReward);
            fightIdx = -1;
            checkLevelUp();
            if (allDefeated()) { phase = WIN; return; }
            phase = OVERWORLD;
            return;
        }

        // Enemy counter-attack
        int edm = max(1, et.atk + (int)random(0, 3) - def);
        hp -= edm;
        snprintf(combatMsg, sizeof(combatMsg), "Hit%d  Rcv%d       ", dmg, edm);
        if (hp <= 0) { hp = 0; phase = GAMEOVER; }
    }

    void flee() {
        if (fightIdx < 0 || phase != COMBAT) return;
        if (random(0, 2) == 0) {
            fightIdx = -1;
            phase    = OVERWORLD;
            showEvent("You escaped!");
            return;
        }
        const EnemyType &et = ETYPES[enemies[fightIdx].type];
        int edm = max(1, et.atk - def);
        hp -= edm;
        snprintf(combatMsg, sizeof(combatMsg), "Flee failed! -%d HP", edm);
        if (hp <= 0) { hp = 0; phase = GAMEOVER; }
    }

    void checkLevelUp() {
        while (level < MAX_LEVEL && xp >= LEVEL_XP[level + 1]) {
            level++;
            maxHp += 3;
            hp = min(hp + 3, maxHp);
            atk++;
            def++;
            showEvent("Level up!");
        }
    }

    bool allDefeated() const {
        for (int i = 0; i < numEnemies; i++)
            if (enemies[i].alive) return false;
        return true;
    }

    // Town purchases
    bool buyInn()      { if (gp < 15) { showEvent("Need 15 GP!"); return false; }
                         gp -= 15; hp = maxHp; showEvent("Fully healed!"); return true; }
    bool buyPotion()   { if (gp < 10) { showEvent("Need 10 GP!"); return false; }
                         gp -= 10; hp = min(hp + 8, maxHp); showEvent("Potion +8 HP"); return true; }
    bool upgradeAtk()  { if (gp < 25) { showEvent("Need 25 GP!"); return false; }
                         gp -= 25; atk++; showEvent("ATK increased!"); return true; }
    bool upgradeDef()  { if (gp < 25) { showEvent("Need 25 GP!"); return false; }
                         gp -= 25; def++; showEvent("DEF increased!"); return true; }
    void leaveTown()   { townId = -1; phase = OVERWORLD; }
};

UltimaGame game;

// ─── Renderer ─────────────────────────────────────────────────────────────────

class UltimaRenderer : public DisplayDrawable<LK204_25_LCD, 4, 20> {
    UltimaGame &_g;

    int viewLeft() const {
        int l = _g.px - VW / 2;
        return max(0, min(l, MAP_W - VW));
    }
    int viewTop() const {
        int t = _g.py - VH / 2;
        return max(0, min(t, MAP_H - VH));
    }

public:
    UltimaRenderer(UltimaGame &g) : _g(g) {}
    bool wantsCursor() override { return false; }
    void cursorPosition(int &col, int &row) override { col = 0; row = 3; }

    void draw(DisplayBuffer<4, 20> &buf) override {
        _g.pollEvent();
        buf.clear(' ');

        switch (_g.phase) {

        case UltimaGame::ATTRACT:
            buf.write(0, 0, "  ** MICROULTIMA ** ", 20);
            buf.write(1, 0, "  Tile RPG adventure", 20);
            buf.write(2, 0, "  Numpad 2/4/6/8 mvt", 20);
            buf.write(3, 0, "   Press any key... ", 20);
            return;

        case UltimaGame::GAMEOVER:
            buf.write(0, 0, " *** GAME OVER ***  ", 20);
            buf.write(1, 0, "You have been slain.", 20);
            buf.write(3, 0, "    * = New Game    ", 20);
            return;

        case UltimaGame::WIN: {
            char r2[21];
            buf.write(0, 0, "  *** YOU WIN! ***  ", 20);
            buf.write(1, 0, "All foes vanquished!", 20);
            snprintf(r2, sizeof(r2), "  Level %d Hero!     ", _g.level);
            buf.write(2, 0, r2, 20);
            buf.write(3, 0, "    * = New Game    ", 20);
            return;
        }

        case UltimaGame::COMBAT: {
            const Enemy &e = _g.enemies[_g.fightIdx];
            const EnemyType &et = ETYPES[e.type];
            char r0[21], r1[21], r2[21], r3[21];
            snprintf(r0, sizeof(r0), "** %-14.14s **", et.name);
            snprintf(r1, sizeof(r1), "Enemy HP: %-10d", e.hp);
            snprintf(r2, sizeof(r2), "%-20.20s",
                _g.combatMsg[0] ? _g.combatMsg : "[A]Attack  [B]Flee");
            snprintf(r3, sizeof(r3), "HP:%d/%d ATK:%d DEF:%d",
                _g.hp, _g.maxHp, _g.atk, _g.def);
            buf.write(0, 0, r0, 20);
            buf.write(1, 0, r1, 20);
            buf.write(2, 0, r2, 20);
            buf.write(3, 0, r3, 20);
            return;
        }

        case UltimaGame::TOWN: {
            const char *tn = (_g.townId == 0) ? "BRIGHTKEEP" : "SHADOWFEN";
            char r0[21], r3[21];
            snprintf(r0, sizeof(r0), "** %-14.14s **", tn);
            snprintf(r3, sizeof(r3), "[B]Leave   GP:%-6d", _g.gp);
            buf.write(0, 0, r0, 20);
            buf.write(1, 0, "[1]Inn:15g [2]Pot:10", 20);
            if (_g.hasEventMsg) {
                char em[21];
                snprintf(em, sizeof(em), "%-20.20s", _g.eventMsg);
                buf.write(2, 0, em, 20);
            } else {
                buf.write(2, 0, "[3]ATK:25g [4]DEF:25", 20);
            }
            buf.write(3, 0, r3, 20);
            return;
        }

        default: break;  // OVERWORLD falls through
        }

        // ── OVERWORLD: 3 map rows + 1 stats row ──────────────────────────────
        int vx0 = viewLeft();
        int vy0 = viewTop();

        for (int row = 0; row < VH; row++) {
            int my = vy0 + row;

            // Timed event message replaces the bottom map row
            if (_g.hasEventMsg && row == VH - 1) {
                char em[21];
                snprintf(em, sizeof(em), "%-20.20s", _g.eventMsg);
                buf.write(row, 0, em, 20);
                continue;
            }

            char line[VW + 1];
            for (int col = 0; col < VW; col++) {
                int mx = vx0 + col;
                line[col] = (mx == _g.px && my == _g.py)
                            ? '@'
                            : _g.displayTile(mx, my);
            }
            line[VW] = '\0';
            buf.write(row, 0, line, VW);
        }

        // Row 3: stats  "HP:15/15 GP:020 Lv:3"
        char stats[21];
        snprintf(stats, sizeof(stats), "HP:%d/%d GP:%d Lv:%d  ",
            _g.hp, _g.maxHp, _g.gp, _g.level);
        buf.write(3, 0, stats, 20);
    }
};

// ─── Encoder handlers ─────────────────────────────────────────────────────────

// Left encoder: move north / south
class NorthSouthEncoder : public EncoderWheelHandler {
    UltimaGame &_g;
public:
    NorthSouthEncoder(Schedule &s, const EncoderConfig &cfg, UltimaGame &g)
        : EncoderWheelHandler(s, cfg), _g(g) {}
    void handleInput(uint8_t input) override {
        if (_g.phase != UltimaGame::OVERWORLD) return;
        _g.tryMove(0, input == ENCODER_WHEEL_RIGHT ? 1 : -1);
    }
};

// Right encoder: move east / west
class EastWestEncoder : public EncoderWheelHandler {
    UltimaGame &_g;
public:
    EastWestEncoder(Schedule &s, const EncoderConfig &cfg, UltimaGame &g)
        : EncoderWheelHandler(s, cfg), _g(g) {}
    void handleInput(uint8_t input) override {
        if (_g.phase != UltimaGame::OVERWORLD) return;
        _g.tryMove(input == ENCODER_WHEEL_RIGHT ? 1 : -1, 0);
    }
};

// ─── Encoder button callbacks ─────────────────────────────────────────────────

static void onLeftClick() {
    if (game.phase == UltimaGame::ATTRACT) game.begin();
}

static void onRightClick() {
    if      (game.phase == UltimaGame::ATTRACT) game.begin();
    else if (game.phase == UltimaGame::COMBAT)  game.attackEnemy();
}

// ─── Keypad handler ───────────────────────────────────────────────────────────

class UltimaKeys : public KeypadKeyHandler {
    UltimaGame &_g;
public:
    UltimaKeys(UltimaGame &g) : _g(g) {}

    bool handle_key(char ch) override {
        if (ch == KeypadKey_Asterisk) { _g.begin(); return true; }

        if (_g.phase == UltimaGame::ATTRACT) { _g.begin(); return true; }

        if (_g.phase == UltimaGame::GAMEOVER || _g.phase == UltimaGame::WIN)
            return false;

        if (_g.phase == UltimaGame::COMBAT) {
            if (ch == KeypadKey_A) { _g.attackEnemy(); return true; }
            if (ch == KeypadKey_B) { _g.flee();        return true; }
            return false;
        }

        if (_g.phase == UltimaGame::TOWN) {
            if (ch == KeypadKey_1) { _g.buyInn();     return true; }
            if (ch == KeypadKey_2) { _g.buyPotion();  return true; }
            if (ch == KeypadKey_3) { _g.upgradeAtk(); return true; }
            if (ch == KeypadKey_4) { _g.upgradeDef(); return true; }
            if (ch == KeypadKey_B) { _g.leaveTown();  return true; }
            return false;
        }

        // Overworld numpad movement
        if (ch == KeypadKey_8) { _g.tryMove( 0, -1); return true; }  // N
        if (ch == KeypadKey_2) { _g.tryMove( 0,  1); return true; }  // S
        if (ch == KeypadKey_4) { _g.tryMove(-1,  0); return true; }  // W
        if (ch == KeypadKey_6) { _g.tryMove( 1,  0); return true; }  // E
        if (ch == KeypadKey_7) { _g.tryMove(-1, -1); return true; }  // NW
        if (ch == KeypadKey_9) { _g.tryMove( 1, -1); return true; }  // NE
        if (ch == KeypadKey_1) { _g.tryMove(-1,  1); return true; }  // SW
        if (ch == KeypadKey_3) { _g.tryMove( 1,  1); return true; }  // SE
        if (ch == KeypadKey_5) { return true; }                       // Wait

        return false;
    }
};

// ─── Hardware setup ───────────────────────────────────────────────────────────

MainSchedule    schedule;
LK204_25_LCD    lcd;
LK204_25_Keypad keypad;

UltimaRenderer renderer(game);
MainDisplay<LK204_25_LCD, 4, 20> display(schedule, lcd, renderer, 100, 5000L);

UltimaKeys   keys(game);
KeypadHandler keypadHandler(schedule, keypad, keys);

NorthSouthEncoder nsEnc(schedule, Config.Left.Encoder,  game);
EastWestEncoder   ewEnc(schedule, Config.Right.Encoder, game);

ButtonHandler leftEncBtn (schedule, Config.Left.Button,  onLeftClick);
ButtonHandler rightEncBtn(schedule, Config.Right.Button, onRightClick);

// ─── Sketch entry points ──────────────────────────────────────────────────────

void setup() {
    keypad.begin();
    display.begin();
    schedule.begin();
    // Game starts in ATTRACT; any key begins play.
}

void loop() {
    schedule.poll();
}
