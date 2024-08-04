#include <string.h>

#include "input.h"
#include "utils.h"

static const char *button_names[] = {
	NULL,
	NULL,
	NULL,
	NULL,
	[INPUT_KEY_A] = "a",
	[INPUT_KEY_B] = "b",
	[INPUT_KEY_C] = "c",
	[INPUT_KEY_D] = "d",
	[INPUT_KEY_E] = "e",
	[INPUT_KEY_F] = "f",
	[INPUT_KEY_G] = "g",
	[INPUT_KEY_H] = "h",
	[INPUT_KEY_I] = "i",
	[INPUT_KEY_J] = "j",
	[INPUT_KEY_K] = "k",
	[INPUT_KEY_L] = "l",
	[INPUT_KEY_M] = "m",
	[INPUT_KEY_N] = "n",
	[INPUT_KEY_O] = "o",
	[INPUT_KEY_P] = "p",
	[INPUT_KEY_Q] = "q",
	[INPUT_KEY_R] = "r",
	[INPUT_KEY_S] = "s",
	[INPUT_KEY_T] = "t",
	[INPUT_KEY_U] = "u",
	[INPUT_KEY_V] = "v",
	[INPUT_KEY_W] = "w",
	[INPUT_KEY_X] = "x",
	[INPUT_KEY_Y] = "y",
	[INPUT_KEY_Z] = "z",
	[INPUT_KEY_1] = "1",
	[INPUT_KEY_2] = "2",
	[INPUT_KEY_3] = "3",
	[INPUT_KEY_4] = "4",
	[INPUT_KEY_5] = "5",
	[INPUT_KEY_6] = "6",
	[INPUT_KEY_7] = "7",
	[INPUT_KEY_8] = "8",
	[INPUT_KEY_9] = "9",
	[INPUT_KEY_0] = "0",
	[INPUT_KEY_RETURN] = "return",
	[INPUT_KEY_ESCAPE] = "escape",
	[INPUT_KEY_BACKSPACE] = "backspace",
	[INPUT_KEY_TAB] = "tab",
	[INPUT_KEY_SPACE] = "space",
	[INPUT_KEY_MINUS] = "minus",
	[INPUT_KEY_EQUALS] = "equals",
	[INPUT_KEY_LEFTBRACKET] = "l_bracket",
	[INPUT_KEY_RIGHTBRACKET] = "r_bracket",
	[INPUT_KEY_BACKSLASH] = "backslash",
	[INPUT_KEY_HASH] = "hash",
	[INPUT_KEY_SEMICOLON] = "semicolon",
	[INPUT_KEY_APOSTROPHE] = "apostrophe",
	[INPUT_KEY_TILDE] = "tilde",
	[INPUT_KEY_COMMA] = "comma",
	[INPUT_KEY_PERIOD] = "period",
	[INPUT_KEY_SLASH] = "slash",
	[INPUT_KEY_CAPSLOCK] = "capslock",
	[INPUT_KEY_F1] = "f1",
	[INPUT_KEY_F2] = "f2",
	[INPUT_KEY_F3] = "f3",
	[INPUT_KEY_F4] = "f4",
	[INPUT_KEY_F5] = "f5",
	[INPUT_KEY_F6] = "f6",
	[INPUT_KEY_F7] = "f7",
	[INPUT_KEY_F8] = "f8",
	[INPUT_KEY_F9] = "f9",
	[INPUT_KEY_F10] = "f10",
	[INPUT_KEY_F11] = "f11",
	[INPUT_KEY_F12] = "f12",
	[INPUT_KEY_PRINTSCREEN] = "printscreen",
	[INPUT_KEY_SCROLLLOCK] = "scrolllock",
	[INPUT_KEY_PAUSE] = "pause",
	[INPUT_KEY_INSERT] = "insert",
	[INPUT_KEY_HOME] = "home",
	[INPUT_KEY_PAGEUP] = "page_up",
	[INPUT_KEY_DELETE] = "delete",
	[INPUT_KEY_END] = "end",
	[INPUT_KEY_PAGEDOWN] = "page_down",
	[INPUT_KEY_RIGHT] = "right",
	[INPUT_KEY_LEFT] = "left",
	[INPUT_KEY_DOWN] = "down",
	[INPUT_KEY_UP] = "up",
	[INPUT_KEY_NUMLOCK] = "numlock",
	[INPUT_KEY_KP_DIVIDE] = "kp_pdivide",
	[INPUT_KEY_KP_MULTIPLY] = "kp_multiply",
	[INPUT_KEY_KP_MINUS] = "kp_minus",
	[INPUT_KEY_KP_PLUS] = "kp_plus",
	[INPUT_KEY_KP_ENTER] = "kp_enter",
	[INPUT_KEY_KP_1] = "kp_1",
	[INPUT_KEY_KP_2] = "kp_2",
	[INPUT_KEY_KP_3] = "kp_3",
	[INPUT_KEY_KP_4] = "kp_4",
	[INPUT_KEY_KP_5] = "kp_5",
	[INPUT_KEY_KP_6] = "kp_6",
	[INPUT_KEY_KP_7] = "kp_7",
	[INPUT_KEY_KP_8] = "kp_8",
	[INPUT_KEY_KP_9] = "kp_9",
	[INPUT_KEY_KP_0] = "kp_0",
	[INPUT_KEY_KP_PERIOD] = "kp_period",

	[INPUT_KEY_L_CTRL] = "l_ctrl",
	[INPUT_KEY_L_SHIFT] = "l_shift",
	[INPUT_KEY_L_ALT] = "l_alt",
	[INPUT_KEY_L_GUI] = "l_gui",
	[INPUT_KEY_R_CTRL] = "r_ctrl",
	[INPUT_KEY_R_SHIFT] = "r_shift",
	[INPUT_KEY_R_ALT] = "r_alt",
	NULL,
	[INPUT_GAMEPAD_A] = "gamepad_a",
	[INPUT_GAMEPAD_Y] = "gamepad_y",
	[INPUT_GAMEPAD_B] = "gamepad_b",
	[INPUT_GAMEPAD_X] = "gamepad_x",
	[INPUT_GAMEPAD_L_SHOULDER] = "gamepad_l_shoulder",
	[INPUT_GAMEPAD_R_SHOULDER] = "gamepad_r_shoulder",
	[INPUT_GAMEPAD_L_TRIGGER] = "gamepad_l_trigger",
	[INPUT_GAMEPAD_R_TRIGGER] = "gamepad_r_trigger",
	[INPUT_GAMEPAD_SELECT] = "gamepad_select",
	[INPUT_GAMEPAD_START] = "gamepad_start",
	[INPUT_GAMEPAD_L_STICK_PRESS] = "gamepad_l_stick",
	[INPUT_GAMEPAD_R_STICK_PRESS] = "gamepad_r_stick",
	[INPUT_GAMEPAD_DPAD_UP] = "gamepad_dp_up",
	[INPUT_GAMEPAD_DPAD_DOWN] = "gamepad_dp_down",
	[INPUT_GAMEPAD_DPAD_LEFT] = "gamepad_dp_left",
	[INPUT_GAMEPAD_DPAD_RIGHT] = "gamepad_dp_right",
	[INPUT_GAMEPAD_HOME] = "gamepad_home",
	[INPUT_GAMEPAD_L_STICK_UP] = "gamepad_l_stick_up",
	[INPUT_GAMEPAD_L_STICK_DOWN] = "gamepad_l_stick_down",
	[INPUT_GAMEPAD_L_STICK_LEFT] = "gamepad_l_stick_left",
	[INPUT_GAMEPAD_L_STICK_RIGHT] = "gamepad_l_stick_right",
	[INPUT_GAMEPAD_R_STICK_UP] = "gamepad_r_stick_up",
	[INPUT_GAMEPAD_R_STICK_DOWN] = "gamepad_r_stick_down",
	[INPUT_GAMEPAD_R_STICK_LEFT] = "gamepad_r_stick_left",
	[INPUT_GAMEPAD_R_STICK_RIGHT] = "gamepad_r_stick_right",
	NULL,
	[INPUT_MOUSE_LEFT] = "mouse_left",
	[INPUT_MOUSE_MIDDLE] = "mouse_middle",
	[INPUT_MOUSE_RIGHT] = "mouse_right",
	[INPUT_MOUSE_WHEEL_UP] = "mouse_wheel_up",
	[INPUT_MOUSE_WHEEL_DOWN] = "mouse_wheel_wdown",
};

static float actions_state[INPUT_ACTION_MAX];
static bool actions_pressed[INPUT_ACTION_MAX];
static bool actions_released[INPUT_ACTION_MAX];

static uint8_t expected_button[INPUT_ACTION_MAX];
static uint8_t bindings[INPUT_BUTTON_MAX];

static input_capture_callback_t capture_callback;
static void *capture_user;

static int32_t mouse_x;
static int32_t mouse_y;

void input_init(void) {
	input_unbind_all();
}

void input_cleanup(void) {

}

void input_clear(void) {
	clear(actions_pressed);
	clear(actions_released);
}

void input_set_button_state(button_t button, float state) {
	error_if(button < 0 || button >= INPUT_BUTTON_MAX, "Invalid input button %d", button);

	uint8_t action = bindings[button];
	if (action == INPUT_ACTION_NONE) {
		return;
	}

	uint8_t expected = expected_button[action];
	if (!expected || expected == button) {
		state = (state > INPUT_DEADZONE) ? state : 0;

		if (state && !actions_state[action]) {
			actions_pressed[action] = true;
			expected_button[action] = button;
		}
		else if (!state && actions_state[action]) {
			actions_released[action] = true;
			expected_button[action] = INPUT_BUTTON_NONE;
		}
		actions_state[action] = state;
	}

	if (capture_callback && state > INPUT_DEADZONE_CAPTURE) {
		capture_callback(capture_user, button, 0);
	}
}

void input_set_mouse_pos(int32_t x, int32_t y) {
	mouse_x = x;
	mouse_y = y;
}

void input_capture(input_capture_callback_t cb, void *user) {
	capture_callback = cb;
	capture_user = user;
	input_clear();
}

void input_textinput(int32_t ascii_char) {
	if (capture_callback) {
		capture_callback(capture_user, INPUT_INVALID, ascii_char);
	}
}

void input_bind(button_t button, uint8_t action) {
	error_if(button < 0 || button >= INPUT_BUTTON_MAX, "Invalid input button %d", button);
	error_if(action < 0 || action >= INPUT_ACTION_MAX, "Invalid input action %d", action);

	actions_state[action] = 0;
	bindings[button] = action;
}

uint8_t input_action_for_button(button_t button) {
	error_if(button < 0 || button >= INPUT_BUTTON_MAX, "Invalid input button %d", button);
	return bindings[button];
}

void input_unbind(button_t button) {
	error_if(button < 0 || button >= INPUT_BUTTON_MAX, "Invalid input button %d", button);

	bindings[button] = INPUT_ACTION_NONE;
}

void input_unbind_all(void) {	
	for (uint32_t button = 0; button < INPUT_BUTTON_MAX; button++) {
		input_unbind(button);
	}
}


float input_state(uint8_t action) {
	error_if(action < 0 || action >= INPUT_ACTION_MAX, "Invalid input action %d", action);
	return actions_state[action];
}


bool input_pressed(uint8_t action) {
	error_if(action < 0 || action >= INPUT_ACTION_MAX, "Invalid input action %d", action);
	return actions_pressed[action];
}


bool input_released(uint8_t action) {
	error_if(action < 0 || action >= INPUT_ACTION_MAX, "Invalid input action %d", action);
	return actions_released[action];
}

vec2_t input_mouse_pos(void) {
	return vec2(mouse_x, mouse_y);
}


button_t input_name_to_button(const char *name) {
	for (int32_t i = 0; i < INPUT_BUTTON_MAX; i++) {
		if (button_names[i] && str_equals(name, button_names[i])) {
			return i;
		}
	}
	return INPUT_INVALID;
}

const char *input_button_to_name(button_t button) {
	if (
		button < 0 || button >= INPUT_BUTTON_MAX ||
		!button_names[button]
	) {
		return NULL;
	}
	return button_names[button];
}
