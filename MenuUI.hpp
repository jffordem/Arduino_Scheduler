#ifndef MENU_UI_HPP
#define MENU_UI_HPP

#include <Scheduler.hpp>
#include <Display.hpp>
#include <KeypadHandler.hpp>
#include <string.h>
#include <stdio.h>
#include <limits.h>

// A small retained-mode menu system designed for character LCDs.
// It renders into DisplayBuffer and is intended to be used with MainDisplay.

enum MenuItemKind {
	MenuItem_Action,
	MenuItem_Submenu,
	MenuItem_ToggleEnabled,
	MenuItem_EditLong,
	MenuItem_EnterLong,
	MenuItem_EnterString
};

class MenuContext;

struct MenuItem {
	const char *label;
	MenuItemKind kind;
	void (*action)(MenuContext &ctx);
	void *target;
	long step;
	int maxLen;
	long shortDelayMs;
	long longDelayMs;

	static MenuItem Action(const char *labelValue, void (*fn)(MenuContext &ctx)) {
		MenuItem i;
		i.label = labelValue;
		i.kind = MenuItem_Action;
		i.action = fn;
		i.target = 0;
		i.step = 1;
		i.maxLen = 0;
		i.shortDelayMs = 750;
		i.longDelayMs = 1500;
		return i;
	}
	static MenuItem Submenu(const char *labelValue, void *menu) {
		MenuItem i;
		i.label = labelValue;
		i.kind = MenuItem_Submenu;
		i.action = 0;
		i.target = menu;
		i.step = 1;
		i.maxLen = 0;
		i.shortDelayMs = 750;
		i.longDelayMs = 1500;
		return i;
	}
	static MenuItem Toggle(const char *labelValue, Enabled &enabled) {
		MenuItem i;
		i.label = labelValue;
		i.kind = MenuItem_ToggleEnabled;
		i.action = 0;
		i.target = &enabled;
		i.step = 1;
		i.maxLen = 0;
		i.shortDelayMs = 750;
		i.longDelayMs = 1500;
		return i;
	}
	static MenuItem EditLong(const char *labelValue, long &value, long stepValue = 10) {
		MenuItem i;
		i.label = labelValue;
		i.kind = MenuItem_EditLong;
		i.action = 0;
		i.target = &value;
		i.step = stepValue;
		i.maxLen = 0;
		i.shortDelayMs = 750;
		i.longDelayMs = 1500;
		return i;
	}
	static MenuItem EnterLong(const char *labelValue, long &value) {
		MenuItem i;
		i.label = labelValue;
		i.kind = MenuItem_EnterLong;
		i.action = 0;
		i.target = &value;
		i.step = 1;
		i.maxLen = 0;
		i.shortDelayMs = 750;
		i.longDelayMs = 1500;
		return i;
	}
	static MenuItem EnterString(const char *labelValue, char *value, int maxLenValue, long shortDelayMsValue = 750, long longDelayMsValue = 1500) {
		MenuItem i;
		i.label = labelValue;
		i.kind = MenuItem_EnterString;
		i.action = 0;
		i.target = value;
		i.step = 1;
		i.maxLen = maxLenValue;
		i.shortDelayMs = shortDelayMsValue;
		i.longDelayMs = longDelayMsValue;
		return i;
	}
};

class MenuScreen {
	const char *_title;
	const MenuItem *_items;
	int _count;
public:
	MenuScreen(const char *title, const MenuItem *items, int count) : _title(title), _items(items), _count(count) { }
	const char *title() const { return _title; }
	const MenuItem &item(int index) const { return _items[index]; }
	int count() const { return _count; }
};

class MenuContext {
	static const int MaxEditString = 32;
	static const int MaxStack = 8;
	const MenuScreen *_stack[MaxStack];
	int _depth;
	int _selected;
	int _top;

	// Edit state
	bool _editing;
	long *_editValue;
	long _editStep;
	const char *_editLabel;
	MenuItemKind _editKind;
	long _editOriginal;
	long _enterValue;
	bool _enterStarted;
	char *_editString;
	int _editStringMax;
	int _editStringPos;
	char _editStringOriginal[MaxEditString];
	char _stringPendingKey;
	int _stringPendingIndex;
	unsigned long _stringLastPress;
	long _stringShortDelay;
	long _stringLongDelay;
public:
	MenuContext(const MenuScreen &root) : _depth(0), _selected(0), _top(0), _editing(false), _editValue(0), _editStep(1), _editLabel(0), _editKind(MenuItem_Action), _editOriginal(0), _enterValue(0), _enterStarted(false), _editString(0), _editStringMax(0), _editStringPos(0), _stringPendingKey(0), _stringPendingIndex(0), _stringLastPress(0), _stringShortDelay(750), _stringLongDelay(1500) {
		_stack[0] = &root;
		_editStringOriginal[0] = 0;
	}

	const MenuScreen &screen() const { return *_stack[_depth]; }
	int selected() const { return _selected; }
	int top() const { return _top; }
	bool editing() const { return _editing; }

	void push(const MenuScreen &screen) {
		if (_depth + 1 < MaxStack) {
			_depth++;
			_stack[_depth] = &screen;
			_selected = 0;
			_top = 0;
		}
	}
	void pop() {
		if (_editing) {
			if (_editKind == MenuItem_EnterLong && _editValue) {
				*_editValue = _editOriginal;
			}
			if (_editKind == MenuItem_EnterString && _editString) {
				strncpy(_editString, _editStringOriginal, (size_t)_editStringMax);
				_editString[_editStringMax - 1] = 0;
			}
			_editing = false;
			_editValue = 0;
			_editLabel = 0;
			_editKind = MenuItem_Action;
			_enterStarted = false;
			_editString = 0;
			_editStringMax = 0;
			_editStringPos = 0;
			_stringPendingKey = 0;
			_stringPendingIndex = 0;
			_stringLastPress = 0;
			return;
		}
		if (_depth > 0) {
			_depth--;
			_selected = 0;
			_top = 0;
		}
	}

	void move(int delta) {
		if (_editing) {
			if (_editKind == MenuItem_EditLong && _editValue) {
				*_editValue += (long)delta * _editStep;
			}
			return;
		}
		int c = screen().count();
		if (c <= 0) return;
		_selected += delta;
		if (_selected < 0) _selected = 0;
		if (_selected >= c) _selected = c - 1;
		// Keep selected visible; assume 4-row display, title consumes row 0.
		const int visible = 3;
		if (_selected < _top) _top = _selected;
		if (_selected >= _top + visible) _top = _selected - visible + 1;
		if (_top < 0) _top = 0;
	}

	void activate() {
		if (_editing) {
			// Select confirms value and exits edit mode
			if (_editKind == MenuItem_EnterLong && _editValue) {
				*_editValue = _enterValue;
			}
			_editing = false;
			_editValue = 0;
			_editLabel = 0;
			_editKind = MenuItem_Action;
			_enterStarted = false;
			_editString = 0;
			_editStringMax = 0;
			_editStringPos = 0;
			_stringPendingKey = 0;
			_stringPendingIndex = 0;
			_stringLastPress = 0;
			return;
		}
		const MenuItem &it = screen().item(_selected);
		switch (it.kind) {
			case MenuItem_Action:
				if (it.action) it.action(*this);
				break;
			case MenuItem_Submenu:
				if (it.target) push(*(const MenuScreen*)it.target);
				break;
			case MenuItem_ToggleEnabled:
				if (it.target) ((Enabled*)it.target)->toggle();
				break;
			case MenuItem_EditLong:
				_editing = true;
				_editValue = (long*)it.target;
				_editStep = it.step;
				_editLabel = it.label;
				_editKind = MenuItem_EditLong;
				_editOriginal = _editValue ? *_editValue : 0;
				break;
			case MenuItem_EnterLong:
				_editing = true;
				_editValue = (long*)it.target;
				_editStep = 1;
				_editLabel = it.label;
				_editKind = MenuItem_EnterLong;
				_editOriginal = _editValue ? *_editValue : 0;
				if (_editOriginal < 0) _editOriginal = 0;
				_enterValue = _editOriginal;
				_enterStarted = false;
				break;
			case MenuItem_EnterString:
				_editing = true;
				_editValue = 0;
				_editStep = 1;
				_editLabel = it.label;
				_editKind = MenuItem_EnterString;
				_editString = (char*)it.target;
				_editStringMax = it.maxLen;
				if (_editStringMax <= 0) _editStringMax = 1;
				if (_editStringMax > MaxEditString) _editStringMax = MaxEditString;
				_stringShortDelay = it.shortDelayMs;
				_stringLongDelay = it.longDelayMs;
				strncpy(_editStringOriginal, _editString ? _editString : "", (size_t)_editStringMax);
				_editStringOriginal[_editStringMax - 1] = 0;
				if (_editString) {
					_editString[_editStringMax - 1] = 0;
					int len = (int)strlen(_editString);
					int maxChars = _editStringMax - 2;
					if (maxChars < 0) maxChars = 0;
					_editStringPos = len;
					if (_editStringPos < 0) _editStringPos = 0;
					if (_editStringPos > maxChars) _editStringPos = maxChars;
				} else {
					_editStringPos = 0;
				}
				_stringPendingKey = 0;
				_stringPendingIndex = 0;
				_stringLastPress = 0;
				break;
		}
	}

	// For keypad A-D: map row selection relative to top (0..2 are visible lines)
	void selectVisibleRow(int row) {
		if (_editing) return;
		int idx = _top + row;
		if (idx >= 0 && idx < screen().count()) {
			_selected = idx;
			activate();
		}
	}

	const char *editLabel() const { return _editLabel; }
	long editValue() const { return _editKind == MenuItem_EnterLong ? _enterValue : (_editValue ? *_editValue : 0); }
	const char *editString() const { return _editString ? _editString : ""; }
	int editStringPos() const { return _editStringPos; }
	MenuItemKind editKind() const { return _editKind; }
	void tick() {
		if (!_editing) return;
		if (_editKind != MenuItem_EnterString) return;
		if (!_editString) return;
		if (!_stringPendingKey) return;
		unsigned long now = millis();
		if ((long)(now - _stringLastPress) > _stringLongDelay) {
			int maxChars = _editStringMax - 2;
			if (maxChars < 0) maxChars = 0;
			if (_editStringPos < maxChars) {
				_editStringPos++;
			}
			_stringPendingKey = 0;
			_stringPendingIndex = 0;
			_stringLastPress = 0;
		}
	}
	bool handleEditKey(char ch) {
		if (!_editing) return false;
		if (_editKind == MenuItem_EnterLong) {
			if (ch == KeypadKey_Pound || ch == KeypadKey_Hash || ch == KeypadKey_Octothorpe) {
				activate();
				return true;
			}
			if (ch == KeypadKey_Asterisk) {
				if (!_enterStarted) {
					_enterValue = _editOriginal;
					_enterStarted = true;
				}
				_enterValue = _enterValue / 10;
				return true;
			}
			int digit = -1;
			if (ch == KeypadKey_0) digit = 0;
			else if (ch == KeypadKey_1) digit = 1;
			else if (ch == KeypadKey_2) digit = 2;
			else if (ch == KeypadKey_3) digit = 3;
			else if (ch == KeypadKey_4) digit = 4;
			else if (ch == KeypadKey_5) digit = 5;
			else if (ch == KeypadKey_6) digit = 6;
			else if (ch == KeypadKey_7) digit = 7;
			else if (ch == KeypadKey_8) digit = 8;
			else if (ch == KeypadKey_9) digit = 9;
			if (digit >= 0) {
				if (!_enterStarted) {
					_enterValue = 0;
					_enterStarted = true;
				}
				if (_enterValue <= (LONG_MAX - digit) / 10) {
					_enterValue = _enterValue * 10 + digit;
				} else {
					_enterValue = LONG_MAX;
				}
				return true;
			}
			return false;
		}
		if (_editKind == MenuItem_EnterString) {
			if (!_editString) return true;
			if (ch == KeypadKey_Pound || ch == KeypadKey_Hash || ch == KeypadKey_Octothorpe) {
				activate();
				return true;
			}
			if (ch == KeypadKey_Asterisk) {
				if (_stringPendingKey) {
					_editString[_editStringPos] = 0;
					_stringPendingKey = 0;
					_stringPendingIndex = 0;
					_stringLastPress = 0;
					return true;
				}
				if (_editStringPos > 0) {
					_editStringPos--;
					_editString[_editStringPos] = 0;
				}
				return true;
			}

			char key = ch;
			const char *opts = 0;
			switch (key) {
				case KeypadKey_0: opts = "0"; break;
				case KeypadKey_1: opts = "1"; break;
				case KeypadKey_2: opts = "2abcABC"; break;
				case KeypadKey_3: opts = "3defDEF"; break;
				case KeypadKey_4: opts = "4ghiGHI"; break;
				case KeypadKey_5: opts = "5jklJKL"; break;
				case KeypadKey_6: opts = "6mnoNBO"; break;
				case KeypadKey_7: opts = "7pqrsPQRS"; break;
				case KeypadKey_8: opts = "8tuvTUV"; break;
				case KeypadKey_9: opts = "9wxyzWXYZ"; break;
				default: opts = 0; break;
			}
			if (!opts) return false;

			unsigned long now = millis();
			long elapsed = _stringLastPress ? (long)(now - _stringLastPress) : MAX_LONG;
			int maxChars = _editStringMax - 2;
			if (maxChars < 0) maxChars = 0;
			if (maxChars < 0) return true;

			if (_stringPendingKey) {
				if (elapsed <= _stringShortDelay) {
					if (key == _stringPendingKey) {
						int n = (int)strlen(opts);
						if (n <= 0) n = 1;
						_stringPendingIndex = (_stringPendingIndex + 1) % n;
						_editString[_editStringPos] = opts[_stringPendingIndex];
						_editString[_editStringMax - 1] = 0;
						_stringLastPress = now;
						return true;
					}
					if (_editStringPos < maxChars) {
						_editStringPos++;
					}
				} else {
					if (_editStringPos < maxChars) {
						_editStringPos++;
					}
				}
			}

			if (_editStringPos >= maxChars) {
				_editStringPos = maxChars;
			}
			int n = (int)strlen(opts);
			if (n <= 0) n = 1;
			_stringPendingKey = key;
			_stringPendingIndex = 0;
			_editString[_editStringPos] = opts[_stringPendingIndex];
			if (_editStringPos + 1 < _editStringMax) {
				_editString[_editStringPos + 1] = 0;
			}
			_editString[_editStringMax - 1] = 0;
			_stringLastPress = now;
			return true;
		}
		return false;
	}
};

struct MenuKeymap {
	char up;
	char down;
	char back;
	char select;
	char line1;
	char line2;
	char line3;
	char line4;
};

// Integrates with KeypadHandler.hpp by acting as a KeypadKeyHandler.
class MenuKeypadController : public KeypadKeyHandler {
	MenuContext &_ctx;
	MenuKeymap _keys;
public:
	MenuKeypadController(MenuContext &ctx, const MenuKeymap &keys) : _ctx(ctx), _keys(keys) { }
	bool handle_key(char ch) {
		if (!ch) return false;
		if (_ctx.handleEditKey(ch)) return true;
		if (ch == _keys.up) { _ctx.move(-1); return true; }
		if (ch == _keys.down) { _ctx.move(+1); return true; }
		if (ch == _keys.back) { _ctx.pop(); return true; }
		if (ch == _keys.select) { _ctx.activate(); return true; }
		if (ch == _keys.line1) { _ctx.selectVisibleRow(0); return true; }
		if (ch == _keys.line2) { _ctx.selectVisibleRow(1); return true; }
		if (ch == _keys.line3) { _ctx.selectVisibleRow(2); return true; }
		if (ch == _keys.line4) { _ctx.selectVisibleRow(3); return true; }
		return false;
	}
};

// Rendering: writes menu text into the buffer. This is the object you pass to MainDisplay.
template <class TDisplay, int TRows = 4, int TCols = 20>
class MenuRenderer : public DisplayDrawable<TDisplay, TRows, TCols> {
	MenuContext &_ctx;
	char _selChar;
public:
	MenuRenderer(MenuContext &ctx, char selectionChar = '>') : _ctx(ctx), _selChar(selectionChar) { }
	bool wantsCursor() override {
		if (!_ctx.editing()) return false;
		MenuItemKind k = _ctx.editKind();
		return k == MenuItem_EnterLong || k == MenuItem_EnterString;
	}
	void cursorPosition(int &col, int &row) override {
		col = 0;
		row = 0;
		if (!_ctx.editing()) return;
		MenuItemKind k = _ctx.editKind();
		if (k == MenuItem_EnterString) {
			row = 1;
			col = _ctx.editStringPos();
			if (col < 0) col = 0;
			if (col >= TCols) col = TCols - 1;
			return;
		}
		if (k == MenuItem_EnterLong) {
			row = 1;
			char temp[32];
			temp[0] = 0;
			snprintf(temp, sizeof(temp), "Enter:%ld", _ctx.editValue());
			col = (int)strlen(temp);
			if (col < 0) col = 0;
			if (col >= TCols) col = TCols - 1;
			return;
		}
	}
	void draw(DisplayBuffer<TRows, TCols> &buffer) {
		_ctx.tick();
		buffer.clear(' ');

		// Header line
		const char *title = _ctx.editing() ? _ctx.editLabel() : _ctx.screen().title();
		if (title) {
			buffer.write(0, 0, title, TCols);
		}

		if (_ctx.editing()) {
			char line[TCols + 1];
			line[0] = 0;
			if (_ctx.editKind() == MenuItem_EnterLong) {
				snprintf(line, sizeof(line), "Enter:%ld", _ctx.editValue());
				buffer.write(1, 0, line, TCols);
				buffer.write(2, 0, "0-9 type *=Del", TCols);
				buffer.write(3, 0, "#=OK Back=Cancel", TCols);
				return;
			}
			if (_ctx.editKind() == MenuItem_EnterString) {
				buffer.write(1, 0, _ctx.editString(), TCols);
				buffer.write(2, 0, "0-9 type *=Del", TCols);
				buffer.write(3, 0, "#=OK Back=Cancel", TCols);
				return;
			}
			snprintf(line, sizeof(line), "Value:%ld", _ctx.editValue());
			buffer.write(1, 0, line, TCols);
			buffer.write(2, 0, "Up/Down change", TCols);
			buffer.write(3, 0, "Select=OK Back=Esc", TCols);
			return;
		}

		// Menu lines: row 0 is header.
		int visible = TRows - 1;
		if (visible <= 0) return;
		for (int i = 0; i < visible; i++) {
			int idx = _ctx.top() + i;
			int row = 1 + i;
			if (idx >= _ctx.screen().count()) break;

			const MenuItem &it = _ctx.screen().item(idx);
			char line[TCols + 1];
			for (int k = 0; k < TCols; k++) line[k] = ' ';
			line[TCols] = 0;

			// Selection marker
			line[0] = (idx == _ctx.selected()) ? _selChar : ' ';
			int cursor = 1;

			// Optional indicator for toggles
			if (it.kind == MenuItem_ToggleEnabled && it.target) {
				bool on = ((Enabled*)it.target)->enabled();
				line[1] = on ? '*' : ' ';
				cursor = 2;
			}

			// Label
			if (it.label) {
				int n = (int)strlen(it.label);
				for (int j = 0; j < n && cursor + j < TCols; j++) {
					line[cursor + j] = it.label[j];
				}
			}
			buffer.write(row, 0, line, TCols);
		}
	}
};

#endif // MENU_UI_HPP
