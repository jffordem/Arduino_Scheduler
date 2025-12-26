// Hi-Low Game

// This is a simple game where the computer picks a random number between
// 1 and 100.  The player gets 8 opportunities to guess the number.  If
// the number is above the player's guess the game gives the hint "higher"
// or if the number is below the player's guess the game gives the hint
// "lower".  If the player guesses the number correctly the game is over
// with "You win!".  If the player runs out of guesses the game is over
// with "You lose!".

#include <Wire.h>

#include <Scheduler.hpp>
#include <Display.hpp>
#include <MenuUI.hpp>
#include <KeypadHandler.hpp>
#include <LK204_25.hpp>

enum HiLoState {
	HiLo_Idle,
	HiLo_Playing,
	HiLo_Won,
	HiLo_Lost
};

static const int HiLo_Min = 1;
static const int HiLo_Max = 100;
static const int HiLo_MaxGuesses = 8;

MainSchedule schedule;

LK204_25_LCD lcd;
LK204_25_Keypad keypad;

static int secretNumber = 0;
static int guessesLeft = HiLo_MaxGuesses;
static HiLoState gameState = HiLo_Idle;

static long guessValue = 50;

static char titleBuf[21];
static char statusBuf[21];

static void updateTitle() {
	if (gameState == HiLo_Playing) {
		snprintf(titleBuf, sizeof(titleBuf), "Hi-Lo %d left", guessesLeft);
		return;
	}
	if (gameState == HiLo_Won) {
		snprintf(titleBuf, sizeof(titleBuf), "Hi-Lo You win!");
		return;
	}
	if (gameState == HiLo_Lost) {
		snprintf(titleBuf, sizeof(titleBuf), "Hi-Lo You lose!");
		return;
	}
	snprintf(titleBuf, sizeof(titleBuf), "Hi-Lo Game");
}

static void setStatus(const char *text) {
	if (!text) text = "";
	snprintf(statusBuf, sizeof(statusBuf), "%s", text);
}

static void newGameInternal() {
	randomSeed(millis());
	secretNumber = random(HiLo_Min, HiLo_Max + 1);
	guessesLeft = HiLo_MaxGuesses;
	gameState = HiLo_Playing;
	guessValue = 50;
	updateTitle();
	setStatus("Make a guess");
}

static void menuBack(MenuContext &ctx) { ctx.pop(); }
static void menuNoop(MenuContext &) { }

static void menuNewGame(MenuContext &) {
	newGameInternal();
}

static void menuSubmitGuess(MenuContext &) {
	if (gameState != HiLo_Playing) {
		if (gameState == HiLo_Won || gameState == HiLo_Lost) {
			setStatus("New game?");
		} else {
			setStatus("Press New game");
		}
		updateTitle();
		return;
	}

	long g = guessValue;
	if (g < HiLo_Min || g > HiLo_Max) {
		setStatus("1-100 only");
		return;
	}

	if ((int)g == secretNumber) {
		gameState = HiLo_Won;
		setStatus("You win!");
		updateTitle();
		return;
	}

	guessesLeft--;
	if (guessesLeft <= 0) {
		guessesLeft = 0;
		gameState = HiLo_Lost;
		char temp[21];
		snprintf(temp, sizeof(temp), "Was %d", secretNumber);
		setStatus(temp);
		updateTitle();
		return;
	}

	setStatus(g < secretNumber ? "Higher" : "Lower");
	updateTitle();
}

MenuItem rootItems[] = {
	MenuItem::Action("New game", &menuNewGame),
	MenuItem::EnterLong("Enter guess", guessValue),
	MenuItem::Action("Submit guess", &menuSubmitGuess),
	MenuItem::Action(statusBuf, &menuNoop),
	MenuItem::Action("Back", &menuBack)
};
MenuScreen rootMenu(titleBuf, rootItems, (int)(sizeof(rootItems) / sizeof(rootItems[0])));

MenuContext menu(rootMenu);
MenuRenderer<LK204_25_LCD, 4, 20> menuRenderer(menu);
MainDisplay<LK204_25_LCD, 4, 20> mainDisplay(schedule, lcd, menuRenderer, 125, 5000L);

MenuKeymap keymap = {
	/*up*/ KeypadKey_2,
	/*down*/ KeypadKey_8,
	/*back*/ KeypadKey_D,
	/*select*/ KeypadKey_A,
	/*line1*/ KeypadKey_A,
	/*line2*/ KeypadKey_B,
	/*line3*/ KeypadKey_C,
	/*line4*/ KeypadKey_D
};
MenuKeypadController menuKeys(menu, keymap);
KeypadHandler keypadHandler(schedule, keypad, menuKeys);

void setup() {
	randomSeed(analogRead(A0));
	updateTitle();
	setStatus("Press New game");

	keypad.begin();
	mainDisplay.begin();

	schedule.begin();
}

void loop() {
	schedule.poll();
}
