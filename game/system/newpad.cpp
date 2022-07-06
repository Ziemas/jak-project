/*!
 * @file newpad.cpp
 * PC-port specific cpad implementation on the C kernel. Monitors button inputs.
 * Actual input detection is done through window events and is gfx pipeline-dependent.
 */

#include "newpad.h"

#include "common/log/log.h"
#include "common/util/Assert.h"
#include <common/util/FileUtil.h>

#include "game/graphics/pipelines/opengl.h"  // for GLFW macros

namespace Pad {

/*
********************************
* Key checking
********************************
*/

constexpr int NUM_KEYS = SDL_NUM_SCANCODES + 1;
// key-down status of any detected key.
bool g_key_status[NUM_KEYS] = {0};
// key-down status of any detected key. this is buffered for the remainder of a frame.
bool g_buffered_key_status[NUM_KEYS] = {0};

bool g_gamepad_buttons[CONTROLLER_COUNT][SDL_CONTROLLER_BUTTON_MAX] = {{0}};
float g_gamepad_analogs[CONTROLLER_COUNT][SDL_CONTROLLER_AXIS_MAX] = {{0}};

struct GamepadState {
  SDL_GameController* gamepad[CONTROLLER_COUNT] = {nullptr, nullptr};
} g_gamepads;

// input mode for controller mapping
InputModeStatus input_mode = InputModeStatus::Disabled;
int input_mode_pad = 0;
u64 input_mode_key = -1;
u64 input_mode_mod = 0;
u64 input_mode_index = 0;
MappingInfo g_input_mode_mapping;

void ForceClearKeys() {
  for (auto& key : g_key_status) {
    key = false;
  }
  for (auto& key : g_buffered_key_status) {
    key = false;
  }
}

void ClearKeys() {
  for (int key = 0; key < NUM_KEYS; key++) {
    g_buffered_key_status[key] = g_key_status[key];
  }
}

void OnKeyPress(int key) {
  if (input_mode == InputModeStatus::Enabled) {
    if (key == SDL_SCANCODE_ESCAPE) {
      ExitInputMode(true);
      return;
    }
    input_mode_key = key;
    MapButton(g_input_mode_mapping, (Button)(input_mode_index++), input_mode_pad, key);
    if (input_mode_index >= (u64)Button::Max) {
      ExitInputMode(false);
    }
    return;
  }
  // set absolute key status
  ASSERT(key < NUM_KEYS);
  g_key_status[key] = true;
  // set buffered key status
  g_buffered_key_status[key] = true;
}

void OnKeyRelease(int key) {
  if (input_mode == InputModeStatus::Enabled) {
    return;
  }
  ASSERT(key < NUM_KEYS);
  g_key_status[key] = false;
}

/*
********************************
* Pad checking
********************************
*/

static int CheckPadIdx(int pad) {
  if (pad < 0 || pad > CONTROLLER_COUNT) {
    lg::error("Invalid pad {}", pad);
    return -1;
  }
  return pad;
}

// returns 1 if button is pressed. returns 0 if invalid or not pressed.
int IsPressed(MappingInfo& mapping, Button button, int pad = 0) {
  if (CheckPadIdx(pad) == -1) {
    return 0;
  }

  if (g_gamepad_buttons[pad][(int)button]) {
    return 1;
  }
  auto key = mapping.pad_mapping[pad][(int)button];
  if (key == -1)
    return 0;
  auto& keymap = mapping.buffer_mode ? g_buffered_key_status : g_key_status;
  ASSERT(key < NUM_KEYS);
  return keymap[key];
}

// returns the value of the analog axis (in the future, likely pressure sensitive if we support it?)
// if invalid or otherwise -- returns 127 (analog stick neutral position)
int AnalogValue(MappingInfo& /*mapping*/, Analog analog, int pad = 0) {
  float input = 0.0f;

  if (CheckPadIdx(pad) == -1) {
    // Pad out of range, return a stable value
    return 127;
  }

  if (pad == 0 && g_gamepads.gamepad[0] == nullptr) {  // Gamepad not present - use keyboard
    // Movement controls mapped to WASD keys
    if (g_buffered_key_status[SDL_SCANCODE_W] && analog == Analog::Left_Y)
      input += -1.0f;
    if (g_buffered_key_status[SDL_SCANCODE_S] && analog == Analog::Left_Y)
      input += 1.0f;
    if (g_buffered_key_status[SDL_SCANCODE_A] && analog == Analog::Left_X)
      input += -1.0f;
    if (g_buffered_key_status[SDL_SCANCODE_D] && analog == Analog::Left_X)
      input += 1.0f;

    // Camera controls mapped to IJKL keys
    if (g_buffered_key_status[SDL_SCANCODE_I] && analog == Analog::Right_Y)
      input += -1.0f;
    if (g_buffered_key_status[SDL_SCANCODE_K] && analog == Analog::Right_Y)
      input += 1.0f;
    if (g_buffered_key_status[SDL_SCANCODE_J] && analog == Analog::Right_X)
      input += -1.0f;
    if (g_buffered_key_status[SDL_SCANCODE_L] && analog == Analog::Right_X)
      input += 1.0f;
  } else if (pad == 1 && g_gamepads.gamepad[1] == nullptr) {
    // these bindings are not sane
    if (g_buffered_key_status[SDL_SCANCODE_KP_5] && analog == Analog::Left_Y)
      input += -1.0f;
    if (g_buffered_key_status[SDL_SCANCODE_KP_2] && analog == Analog::Left_Y)
      input += 1.0f;
    if (g_buffered_key_status[SDL_SCANCODE_KP_1] && analog == Analog::Left_X)
      input += -1.0f;
    if (g_buffered_key_status[SDL_SCANCODE_KP_3] && analog == Analog::Left_X)
      input += 1.0f;

    // these bindings are not sane
    if (g_buffered_key_status[SDL_SCANCODE_KP_DIVIDE] && analog == Analog::Right_Y)
      input += -1.0f;
    if (g_buffered_key_status[SDL_SCANCODE_KP_8] && analog == Analog::Right_Y)
      input += 1.0f;
    if (g_buffered_key_status[SDL_SCANCODE_KP_7] && analog == Analog::Right_X)
      input += -1.0f;
    if (g_buffered_key_status[SDL_SCANCODE_KP_9] && analog == Analog::Right_X)
      input += 1.0f;
  } else {  // Gamepad present
    input = g_gamepad_analogs[pad][(int)analog];
  }

  // GLFW provides float in range -1 to 1, caller expects 0-255
  const float input_low = -1.0f;
  const float input_high = 1.0f;
  const int output_low = 0;
  const int output_high = 255;

  // Map from input to output range
  return int((input - input_low) * (output_high - output_low) / (input_high - input_low) +
             output_low);
}

// map a button on a pad to a key
void MapButton(MappingInfo& mapping, Button button, int pad, int key) {
  // check if pad is valid. dont map buttons with invalid pads.
  if (CheckPadIdx(pad) == -1) {
    return;
  }

  mapping.pad_mapping[pad][(int)button] = key;
}

// reset button mappings
void DefaultMapping(MappingInfo& mapping) {
  // make every button invalid
  for (int p = 0; p < CONTROLLER_COUNT; ++p) {
    for (int i = 0; i < (int)Button::Max; ++i) {
      MapButton(mapping, (Button)i, p, -1);
    }
  }

  // TODO - these are different from the analog bindings above and cause
  // the keyboard to be bound to controls regardless
  //
  // Need someway to toggle off -- where do we have access to the game's settings?

  // R1 / L1
  MapButton(mapping, Button::L1, 0, SDL_SCANCODE_Q);
  MapButton(mapping, Button::R1, 0, SDL_SCANCODE_O);

  // R2 / L2
  MapButton(mapping, Button::L2, 0, SDL_SCANCODE_1);
  MapButton(mapping, Button::R2, 0, SDL_SCANCODE_P);

  // face buttons
  MapButton(mapping, Button::Ecks, 0, SDL_SCANCODE_SPACE);
  MapButton(mapping, Button::Square, 0, SDL_SCANCODE_F);
  MapButton(mapping, Button::Triangle, 0, SDL_SCANCODE_R);
  MapButton(mapping, Button::Circle, 0, SDL_SCANCODE_E);

  // dpad
  MapButton(mapping, Button::Up, 0, SDL_SCANCODE_UP);
  MapButton(mapping, Button::Right, 0, SDL_SCANCODE_RIGHT);
  MapButton(mapping, Button::Down, 0, SDL_SCANCODE_DOWN);
  MapButton(mapping, Button::Left, 0, SDL_SCANCODE_LEFT);

  // start for progress
  MapButton(mapping, Button::Start, 0, SDL_SCANCODE_RETURN);

  // l3/r3 for menu
  MapButton(mapping, Button::L3, 0, SDL_SCANCODE_COMMA);
  MapButton(mapping, Button::R3, 0, SDL_SCANCODE_PERIOD);
}

void EnterInputMode() {
  input_mode = InputModeStatus::Enabled;
  input_mode_index = 0;
  input_mode_pad = 0;
}

void ExitInputMode(bool canceled) {
  input_mode = canceled ? InputModeStatus::Canceled : InputModeStatus::Disabled;
}

u64 input_mode_get() {
  return (u64)input_mode;
}

u64 input_mode_get_key() {
  return input_mode_key;
}

u64 input_mode_get_index() {
  return input_mode_index;
}

void input_mode_pad_set(s64 idx) {
  input_mode_pad = idx;
}

/*
********************************
* Gamepad Support
********************************
*/

void open_controller(int idx) {
  SDL_GameController* controller = SDL_GameControllerOpen(idx);
  if (!controller) {
    lg::error("Failed to open controller %d", idx);
    return;
  }

  lg::info("Opened controller: %s", SDL_GameControllerName(controller));
  g_gamepads.gamepad[0] = controller;
}

void close_controller(int instance) {
  // SDL_GameController* controller = SDL_GameControllerFromInstanceID(instance);
}

void initialize() {
  std::string mapping_path =
      (file_util::get_jak_project_dir() / "game" / "assets" / "sdl_controller_db.txt").string();
  SDL_GameControllerAddMappingsFromFile(file_util::read_text_file(mapping_path).c_str());
}

void clear_pad(int pad) {
  for (int i = 0; i < (int)Button::Max; ++i) {
    g_gamepad_buttons[pad][i] = false;
  }
  for (int i = 0; i < 4; ++i) {
    g_gamepad_analogs[pad][i] = 0;
  }
}


static constexpr std::array<Analog, SDL_CONTROLLER_AXIS_MAX> get_axis_map() {
  std::array<Analog, SDL_CONTROLLER_AXIS_MAX> map{};
  map[SDL_CONTROLLER_AXIS_LEFTX] = Analog::Left_X;
  map[SDL_CONTROLLER_AXIS_LEFTY] = Analog::Left_Y;
  map[SDL_CONTROLLER_AXIS_RIGHTX] = Analog::Right_X;
  map[SDL_CONTROLLER_AXIS_RIGHTY] = Analog::Right_Y;

  return map;
}

void handle_axis_event(SDL_ControllerAxisEvent& evt) {
  if (evt.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT) {
    g_gamepad_buttons[0][(int)Button::L2] = evt.value > 0;
    return;
  }
  if (evt.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
    g_gamepad_buttons[0][(int)Button::R2] = evt.value > 0;
    return;
  }

  constexpr auto map = get_axis_map();
  int axis = static_cast<int>(map[evt.axis]);
  if (axis >= (int)Analog::Max)
    return;

  const float value = static_cast<float>(evt.value) / (evt.value < 0 ? 32768.0f : 32767.0f);
  g_gamepad_analogs[0][axis] = value;
}

static constexpr std::array<Button, SDL_CONTROLLER_BUTTON_MAX> get_button_map() {
  std::array<Button, SDL_CONTROLLER_BUTTON_MAX> map{};
  map[SDL_CONTROLLER_BUTTON_A] = Button::X;
  map[SDL_CONTROLLER_BUTTON_B] = Button::Circle;
  map[SDL_CONTROLLER_BUTTON_X] = Button::Square;
  map[SDL_CONTROLLER_BUTTON_Y] = Button::Triangle;
  map[SDL_CONTROLLER_BUTTON_BACK] = Button::Select;
  map[SDL_CONTROLLER_BUTTON_START] = Button::Start;
  map[SDL_CONTROLLER_BUTTON_LEFTSTICK] = Button::L3;
  map[SDL_CONTROLLER_BUTTON_RIGHTSTICK] = Button::R3;
  map[SDL_CONTROLLER_BUTTON_LEFTSHOULDER] = Button::L1;
  map[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] = Button::R1;
  map[SDL_CONTROLLER_BUTTON_DPAD_UP] = Button::Up;
  map[SDL_CONTROLLER_BUTTON_DPAD_DOWN] = Button::Down;
  map[SDL_CONTROLLER_BUTTON_DPAD_LEFT] = Button::Left;
  map[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] = Button::Right;
  return map;
}

void handle_button_event(SDL_ControllerButtonEvent& evt) {
  constexpr auto map = get_button_map();
  int button = static_cast<int>(map[evt.button]);
  if (button >= (int)Button::Max)
    return;

  g_gamepad_buttons[0][button] = evt.state;
}

void update_gamepads() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
      case SDL_KEYDOWN:
        OnKeyPress(event.key.keysym.scancode);
        break;
      case SDL_KEYUP:
        OnKeyRelease(event.key.keysym.scancode);
        break;
      case SDL_CONTROLLERBUTTONDOWN:
      case SDL_CONTROLLERBUTTONUP:
        handle_button_event(event.cbutton);
        break;
      case SDL_CONTROLLERAXISMOTION:
        handle_axis_event(event.caxis);
        break;
      case SDL_CONTROLLERDEVICEADDED:
        open_controller(event.cdevice.which);
        break;
      case SDL_CONTROLLERDEVICEREMOVED:
        close_controller(event.cdevice.which);
        break;
    }
  }
}

int rumble(int pad, u16 slow_motor, u16 fast_motor) {
  if (g_gamepads.gamepad[pad] != nullptr) {
    SDL_GameControllerRumble(g_gamepads.gamepad[pad], slow_motor, fast_motor, 2000);
    return 1;
  }
  return 0;
}

};  // namespace Pad
