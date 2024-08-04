#ifndef HI_INPUT_H
#define HI_INPUT_H

// This abstract input handling from different input devices to actions. One 
// ore more keys or buttons can be "bound" to the same action in your game.

#include "types.h"

// The deadzone in the normalized 0..1 range in which button presses are 
// ignored. This only takes effect for "analog" input, such as sticks on a game
// controller.
#if !defined(INPUT_DEADZONE)
	#define INPUT_DEADZONE 0.1
#endif

// The deadzone for input_capture()
#if !defined(INPUT_DEADZONE_CAPTURE)
	#define INPUT_DEADZONE_CAPTURE 0.5
#endif

// The maximum number of discrete actions
#if !defined(INPUT_ACTION_MAX)
	#define INPUT_ACTION_MAX 32
#endif

#define INPUT_ACTION_NONE 255
#define INPUT_BUTTON_NONE 0

// Key and buttons names for input_bind() 
typedef enum {
	INPUT_INVALID = 0,
	INPUT_KEY_A = 4,
	INPUT_KEY_B = 5,
	INPUT_KEY_C = 6,
	INPUT_KEY_D = 7,
	INPUT_KEY_E = 8,
	INPUT_KEY_F = 9,
	INPUT_KEY_G = 10,
	INPUT_KEY_H = 11,
	INPUT_KEY_I = 12,
	INPUT_KEY_J = 13,
	INPUT_KEY_K = 14,
	INPUT_KEY_L = 15,
	INPUT_KEY_M = 16,
	INPUT_KEY_N = 17,
	INPUT_KEY_O = 18,
	INPUT_KEY_P = 19,
	INPUT_KEY_Q = 20,
	INPUT_KEY_R = 21,
	INPUT_KEY_S = 22,
	INPUT_KEY_T = 23,
	INPUT_KEY_U = 24,
	INPUT_KEY_V = 25,
	INPUT_KEY_W = 26,
	INPUT_KEY_X = 27,
	INPUT_KEY_Y = 28,
	INPUT_KEY_Z = 29,
	INPUT_KEY_1 = 30,
	INPUT_KEY_2 = 31,
	INPUT_KEY_3 = 32,
	INPUT_KEY_4 = 33,
	INPUT_KEY_5 = 34,
	INPUT_KEY_6 = 35,
	INPUT_KEY_7 = 36,
	INPUT_KEY_8 = 37,
	INPUT_KEY_9 = 38,
	INPUT_KEY_0 = 39,
	INPUT_KEY_RETURN = 40,
	INPUT_KEY_ESCAPE = 41,
	INPUT_KEY_BACKSPACE = 42,
	INPUT_KEY_TAB = 43,
	INPUT_KEY_SPACE = 44,
	INPUT_KEY_MINUS = 45,
	INPUT_KEY_EQUALS = 46,
	INPUT_KEY_LEFTBRACKET = 47,
	INPUT_KEY_RIGHTBRACKET = 48,
	INPUT_KEY_BACKSLASH = 49,
	INPUT_KEY_HASH = 50,
	INPUT_KEY_SEMICOLON = 51,
	INPUT_KEY_APOSTROPHE = 52,
	INPUT_KEY_TILDE = 53,
	INPUT_KEY_COMMA = 54,
	INPUT_KEY_PERIOD = 55,
	INPUT_KEY_SLASH = 56,
	INPUT_KEY_CAPSLOCK = 57,
	INPUT_KEY_F1 = 58,
	INPUT_KEY_F2 = 59,
	INPUT_KEY_F3 = 60,
	INPUT_KEY_F4 = 61,
	INPUT_KEY_F5 = 62,
	INPUT_KEY_F6 = 63,
	INPUT_KEY_F7 = 64,
	INPUT_KEY_F8 = 65,
	INPUT_KEY_F9 = 66,
	INPUT_KEY_F10 = 67,
	INPUT_KEY_F11 = 68,
	INPUT_KEY_F12 = 69,
	INPUT_KEY_PRINTSCREEN = 70,
	INPUT_KEY_SCROLLLOCK = 71,
	INPUT_KEY_PAUSE = 72,
	INPUT_KEY_INSERT = 73,
	INPUT_KEY_HOME = 74,
	INPUT_KEY_PAGEUP = 75,
	INPUT_KEY_DELETE = 76,
	INPUT_KEY_END = 77,
	INPUT_KEY_PAGEDOWN = 78,
	INPUT_KEY_RIGHT = 79,
	INPUT_KEY_LEFT = 80,
	INPUT_KEY_DOWN = 81,
	INPUT_KEY_UP = 82,
	INPUT_KEY_NUMLOCK = 83,
	INPUT_KEY_KP_DIVIDE = 84,
	INPUT_KEY_KP_MULTIPLY = 85,
	INPUT_KEY_KP_MINUS = 86,
	INPUT_KEY_KP_PLUS = 87,
	INPUT_KEY_KP_ENTER = 88,
	INPUT_KEY_KP_1 = 89,
	INPUT_KEY_KP_2 = 90,
	INPUT_KEY_KP_3 = 91,
	INPUT_KEY_KP_4 = 92,
	INPUT_KEY_KP_5 = 93,
	INPUT_KEY_KP_6 = 94,
	INPUT_KEY_KP_7 = 95,
	INPUT_KEY_KP_8 = 96,
	INPUT_KEY_KP_9 = 97,
	INPUT_KEY_KP_0 = 98,
	INPUT_KEY_KP_PERIOD = 99,

	INPUT_KEY_L_CTRL = 100,
	INPUT_KEY_L_SHIFT = 101,
	INPUT_KEY_L_ALT = 102,
	INPUT_KEY_L_GUI = 103,
	INPUT_KEY_R_CTRL = 104,
	INPUT_KEY_R_SHIFT = 105,
	INPUT_KEY_R_ALT = 106,

	INPUT_KEY_MAX = 107,

	INPUT_GAMEPAD_A = 108,
	INPUT_GAMEPAD_Y = 109,
	INPUT_GAMEPAD_B = 110,
	INPUT_GAMEPAD_X = 111,
	INPUT_GAMEPAD_L_SHOULDER = 112,
	INPUT_GAMEPAD_R_SHOULDER = 113,
	INPUT_GAMEPAD_L_TRIGGER = 114,
	INPUT_GAMEPAD_R_TRIGGER = 115,
	INPUT_GAMEPAD_SELECT = 116,
	INPUT_GAMEPAD_START = 117,
	INPUT_GAMEPAD_L_STICK_PRESS = 118,
	INPUT_GAMEPAD_R_STICK_PRESS = 119,
	INPUT_GAMEPAD_DPAD_UP = 120,
	INPUT_GAMEPAD_DPAD_DOWN = 121,
	INPUT_GAMEPAD_DPAD_LEFT = 122,
	INPUT_GAMEPAD_DPAD_RIGHT = 123,
	INPUT_GAMEPAD_HOME = 124,
	INPUT_GAMEPAD_L_STICK_UP = 125,
	INPUT_GAMEPAD_L_STICK_DOWN = 126,
	INPUT_GAMEPAD_L_STICK_LEFT = 127,
	INPUT_GAMEPAD_L_STICK_RIGHT = 128,
	INPUT_GAMEPAD_R_STICK_UP = 129,
	INPUT_GAMEPAD_R_STICK_DOWN = 130,
	INPUT_GAMEPAD_R_STICK_LEFT = 131,
	INPUT_GAMEPAD_R_STICK_RIGHT = 132,

	INPUT_MOUSE_LEFT = 134,
	INPUT_MOUSE_MIDDLE = 135,
	INPUT_MOUSE_RIGHT = 136,
	INPUT_MOUSE_WHEEL_UP = 137,
	INPUT_MOUSE_WHEEL_DOWN = 138,

	INPUT_BUTTON_MAX = 139
} button_t;


// Bind a key/button to an action. Multiple buttons can be bound to the same 
// action, but one key/button can only be bound to one action. Action is just
// a uint8_t identifier, usually from an enum in your game.
void input_bind(button_t button, uint8_t action);

// Unbind a key/button 
void input_unbind(button_t button);

// Unbind all keys/buttons
void input_unbind_all(void);

// Return the currently bound action for a key/button. Returns INPUT_ACTION_NONE
// when the button is not bound.
uint8_t input_action_for_button(button_t button);

// Returns the button_t enum for a given name. See input.c for all possible
// names. This is useful when e.g. loading a json config file.
button_t input_name_to_button(const char *name);

// Returns the name of a button.
const char *input_button_to_name(button_t button);

// Returns the current state for an action. For discrete buttons and keyboard
// keys, this is either 0 or 1. For analog input, it is anywhere between
// 0..1.
float input_state(uint8_t action);

// Whether a button for that action was just pressed down before this frame
bool input_pressed(uint8_t action);

// Wheter a button for that action was just released bofere this frame
bool input_released(uint8_t action);

// The current mouse position in real pixels
vec2_t input_mouse_pos(void);

// Set up a capture callback that will receive ALL key and button presses. For
// non-text input, ascii_char will be 0. Call input_capture(NULL, NULL) to 
// uninstall a callback.
typedef void(*input_capture_callback_t)
	(void *user, button_t button, int32_t ascii_char);
void input_capture(input_capture_callback_t cb, void *user);


// Called by the platform
void input_init(void);
void input_cleanup(void);
void input_clear(void);
void input_set_button_state(button_t button, float state);
void input_set_mouse_pos(int32_t x, int32_t y);
void input_textinput(int32_t ascii_char);

#endif
