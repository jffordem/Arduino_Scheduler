#ifndef MENU_UI_HPP
#define MENU_UI_HPP

#include <Scheduler.hpp>
#include <Display.hpp>
#include <KeypadHandler.hpp>
#include <string.h>
#include <stdio.h>

// A small retained-mode menu system designed for character LCDs.
// It renders into DisplayBuffer and is intended to be used with MainDisplay.

enum MenuItemKind {
	MenuItem_Action,
	MenuItem_Submenu,
	MenuItem_ToggleEnabled,
	MenuItem_EditLong
};

class MenuContext;

struct MenuItem {
	const char *label;
	MenuItemKind kind;
	void (*action)(MenuContext &ctx);
	void *target;
	long step;

	static MenuItem Action(const char *labelValue, void (*fn)(MenuContext &ctx)) {
		MenuItem i;
		i.label = labelValue;
		i.kind = MenuItem_Action;
		i.action = fn;
		i.target = 0;
		i.step = 1;
		return i;
	}
	static MenuItem Submenu(const char *labelValue, void *menu) {
		MenuItem i;
		i.label = labelValue;
		i.kind = MenuItem_Submenu;
		i.action = 0;
		i.target = menu;
		i.step = 1;
		return i;
	}
	static MenuItem Toggle(const char *labelValue, Enabled &enabled) {
		MenuItem i;
		i.label = labelValue;
		i.kind = MenuItem_ToggleEnabled;
		i.action = 0;
		i.target = &enabled;
		i.step = 1;
		return i;
	}
	static MenuItem EditLong(const char *labelValue, long &value, long stepValue = 10) {
		MenuItem i;
		i.label = labelValue;
		i.kind = MenuItem_EditLong;
		i.action = 0;
		i.target = &value;
		i.step = stepValue;
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
public:
	MenuContext(const MenuScreen &root) : _depth(0), _selected(0), _top(0), _editing(false), _editValue(0), _editStep(1), _editLabel(0) {
		_stack[0] = &root;
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
			_editing = false;
			_editValue = 0;
			_editLabel = 0;
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
			if (_editValue) {
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
			_editing = false;
			_editValue = 0;
			_editLabel = 0;
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
	long editValue() const { return _editValue ? *_editValue : 0; }
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
	void draw(DisplayBuffer<TRows, TCols> &buffer) {
		buffer.clear(' ');

		// Header line
		const char *title = _ctx.editing() ? _ctx.editLabel() : _ctx.screen().title();
		if (title) {
			buffer.write(0, 0, title, TCols);
		}

		if (_ctx.editing()) {
			char line[TCols + 1];
			line[0] = 0;
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
			int cursor = 2;

			// Optional indicator for toggles
			if (it.kind == MenuItem_ToggleEnabled && it.target) {
				bool on = ((Enabled*)it.target)->enabled();
				line[2] = on ? '*' : ' ';
				cursor = 4;
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
