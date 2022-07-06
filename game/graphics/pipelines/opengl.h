#pragma once

/*!
 * @file opengl.h
 * OpenGL includes.
 */

#define GLFW_INCLUDE_NONE
#include "game/graphics/display.h"
#include "game/graphics/gfx.h"

#include "third-party/glad/include/glad/glad.h"
#include "SDL.h"

enum SDLKeyAction {
  Release = SDL_RELEASED,  // falling edge of key press
  Press = SDL_PRESSED,     // rising edge of key press
  // Repeat = GLFW_REPEAT     // repeated input on hold e.g. when typing something
};

class GLDisplay : public GfxDisplay {
  SDL_Window* m_window;

 public:
  GLDisplay(SDL_Window* window, bool is_main);
  virtual ~GLDisplay();

  bool should_quit = false;
  void* get_window() const { return m_window; }
  void get_position(int* x, int* y);
  void get_size(int* w, int* h);
  void get_scale(float* x, float* y);
  void get_screen_size(int vmode_idx, s32* w, s32* h);
  int get_screen_rate(int vmode_idx);
  int get_screen_vmode_count();
  GfxDisplayMode get_fullscreen();
  void set_size(int w, int h);
  void update_fullscreen(GfxDisplayMode mode, int screen);
  void render();
  bool minimized();
  void set_lock(bool lock);
};

extern const GfxRendererModule gRendererOpenGL;
