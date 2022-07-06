/*!
 * @file opengl.cpp
 * Lower-level OpenGL interface. No actual rendering is performed here!
 */

#include "opengl.h"

#include <condition_variable>
#include <memory>
#include <mutex>

#include "common/dma/dma_copy.h"
#include "common/global_profiler/GlobalProfiler.h"
#include "common/goal_constants.h"
#include "common/log/log.h"
#include "common/util/FileUtil.h"
#include "common/util/FrameLimiter.h"
#include "common/util/Timer.h"
#include "common/util/compress.h"

#include "game/graphics/display.h"
#include "game/graphics/gfx.h"
#include "game/graphics/opengl_renderer/OpenGLRenderer.h"
#include "game/graphics/opengl_renderer/debug_gui.h"
#include "game/graphics/texture/TexturePool.h"
#include "game/runtime.h"
#include "game/system/newpad.h"

#include "third-party/imgui/imgui.h"
#include "third-party/imgui/imgui_impl_opengl3.h"
#define STBI_WINDOWS_UTF8
#include "third-party/imgui/imgui_impl_sdl.h"
#include "third-party/stb_image/stb_image.h"

namespace {

constexpr bool run_dma_copy = false;

struct GraphicsData {
  // vsync
  std::mutex sync_mutex;
  std::condition_variable sync_cv;

  // dma chain transfer
  std::mutex dma_mutex;
  std::condition_variable dma_cv;
  u64 frame_idx = 0;
  u64 frame_idx_of_input_data = 0;
  bool has_data_to_render = false;
  FixedChunkDmaCopier dma_copier;

  // texture pool
  std::shared_ptr<TexturePool> texture_pool;

  std::shared_ptr<Loader> loader;

  // temporary opengl renderer
  OpenGLRenderer ogl_renderer;

  OpenGlDebugGui debug_gui;

  FrameLimiter frame_limiter;
  Timer engine_timer;
  double last_engine_time = 1. / 60.;
  float pmode_alp = 0.f;

  std::string imgui_log_filename, imgui_filename;
  GameVersion version;

  GraphicsData(GameVersion version)
      : dma_copier(EE_MAIN_MEM_SIZE),
        texture_pool(std::make_shared<TexturePool>()),
        loader(std::make_shared<Loader>(file_util::get_jak_project_dir() / "out" /
                                        game_version_names[version] / "fr3")),
        ogl_renderer(texture_pool, loader),
        version(version) {}
};

std::unique_ptr<GraphicsData> g_gfx_data;

bool HasError() {
  return false;
  const char* ptr = SDL_GetError();
  if (strlen(ptr)) {
    lg::error("glfw error: {}", ptr);
    return true;
  } else {
    return false;
  }
}
}  // namespace

static bool gl_inited = false;
static int gl_init(GfxSettings& settings) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC) !=
      0) {
    lg::error("SDL_Init error");
    return 1;
  }

  // request an OpenGL 4.3 Core context
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  // debug check
  if (settings.debug) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
  }

  // glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
  // glfwWindowHint(GLFW_SAMPLES, 1);
  // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  return 0;
}

static void gl_exit() {
  g_gfx_data.reset();
  SDL_Quit();
  gl_inited = false;
}

static std::shared_ptr<GfxDisplay> gl_make_display(int width,
                                                   int height,
                                                   const char* title,
                                                   GfxSettings& settings,
                                                   GameVersion game_version,
                                                   bool is_main) {
  SDL_Window* window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

  if (!window) {
    lg::error("gl_make_display failed - Could not create display window");
    return NULL;
  }

  SDL_GLContext context = SDL_GL_CreateContext(window);
  SDL_GL_MakeCurrent(window, context);

  if (!gl_inited) {
    gladLoadGLLoader(SDL_GL_GetProcAddress);
    if (!gladLoadGL()) {
      lg::error("GL init fail");
      return NULL;
    }
    g_gfx_data = std::make_unique<GraphicsData>(game_version);

    gl_inited = true;
  }

  // window icon
  std::string image_path =
      (file_util::get_jak_project_dir() / "game" / "assets" / "appicon.png").string();

  SDL_Surface* image;
  int w, h;
  auto load_result = stbi_load(image_path.c_str(), &w, &h, nullptr, STBI_rgb_alpha);
  if (load_result) {
    image =
        SDL_CreateRGBSurfaceWithFormatFrom(load_result, w, h, 32, 4 * w, SDL_PIXELFORMAT_RGBA32);
    SDL_SetWindowIcon(window, image);
    SDL_FreeSurface(image);
    stbi_image_free(load_result);
  } else {
    lg::error("Could not load icon for OpenGL window");
  }

  Pad::initialize();

  if (HasError()) {
    lg::error("gl_make_display error");
    return NULL;
  }

  auto display = std::make_shared<GLDisplay>(window, is_main);
  // lg::debug("init display #x{:x}", (uintptr_t)display);

  // setup imgui

  // check that version of the library is okay
  IMGUI_CHECKVERSION();

  // this does initialization for stuff like the font data
  ImGui::CreateContext();

  // Init ImGui settings
  g_gfx_data->imgui_filename = file_util::get_file_path({"imgui.ini"});
  g_gfx_data->imgui_log_filename = file_util::get_file_path({"imgui_log.txt"});
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = g_gfx_data->imgui_filename.c_str();
  io.LogFilename = g_gfx_data->imgui_log_filename.c_str();

  // set up to get inputs for this window
  ImGui_ImplSDL2_InitForOpenGL(window, context);

  // NOTE: imgui's setup calls functions that may fail intentionally, and attempts to disable error
  // reporting so these errors are invisible. But it does not work, and some weird X11 default
  // cursor error is set here that we clear.
  SDL_ClearError();

  auto check_exit = [](void* data, SDL_Event* evt) {
    auto display = (GLDisplay*)data;
    if (evt->type == SDL_QUIT) {
      display->should_quit = true;
    }

    if (evt->type == SDL_KEYUP) {
      if (evt->key.keysym.sym == SDLK_LALT) {
        display->set_imgui_visible(!display->is_imgui_visible());
      }
    }

    if (!display->should_quit) {
      ImGui_ImplSDL2_ProcessEvent(evt);
    }

    return 1;
  };

  // HACK
  SDL_AddEventWatch(check_exit, display.get());

  // set up the renderer
  ImGui_ImplOpenGL3_Init("#version 430");

  return std::static_pointer_cast<GfxDisplay>(display);
}

GLDisplay::GLDisplay(SDL_Window* window, bool is_main) : m_window(window) {
  m_main = is_main;
  // glfwSetWindowUserPointer(window, reinterpret_cast<void*>(this));
}

GLDisplay::~GLDisplay() {
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  // glfwSetWindowUserPointer(m_window, nullptr);
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  SDL_DestroyWindow(m_window);
  if (m_main) {
    gl_exit();
  }
}

namespace {
std::string make_output_file_name(const std::string& file_name) {
  file_util::create_dir_if_needed(file_util::get_file_path({"gfx_dumps"}));
  return file_util::get_file_path({"gfx_dumps", file_name});
}
}  // namespace

static bool endsWith(std::string_view str, std::string_view suffix) {
  return str.size() >= suffix.size() &&
         0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

void render_game_frame(int game_width,
                       int game_height,
                       int window_fb_width,
                       int window_fb_height,
                       int draw_region_width,
                       int draw_region_height,
                       int msaa_samples,
                       bool windows_borderless_hack) {
  // wait for a copied chain.
  bool got_chain = false;
  {
    auto p = scoped_prof("wait-for-dma");
    std::unique_lock<std::mutex> lock(g_gfx_data->dma_mutex);
    // note: there's a timeout here. If the engine is messed up and not sending us frames,
    // we still want to run the glfw loop.
    got_chain = g_gfx_data->dma_cv.wait_for(lock, std::chrono::milliseconds(50),
                                            [=] { return g_gfx_data->has_data_to_render; });
  }
  // render that chain.
  if (got_chain) {
    g_gfx_data->frame_idx_of_input_data = g_gfx_data->frame_idx;
    RenderOptions options;
    options.game_res_w = game_width;
    options.game_res_h = game_height;
    options.window_framebuffer_width = window_fb_width;
    options.window_framebuffer_height = window_fb_height;
    options.draw_region_width = draw_region_width;
    options.draw_region_height = draw_region_height;
    options.msaa_samples = msaa_samples;
    options.draw_render_debug_window = g_gfx_data->debug_gui.should_draw_render_debug();
    options.draw_profiler_window = g_gfx_data->debug_gui.should_draw_profiler();
    options.draw_subtitle_editor_window = g_gfx_data->debug_gui.should_draw_subtitle_editor();
    options.save_screenshot = false;
    options.gpu_sync = g_gfx_data->debug_gui.should_gl_finish();
    options.borderless_windows_hacks = windows_borderless_hack;
    if (g_gfx_data->debug_gui.get_screenshot_flag()) {
      options.save_screenshot = true;
      options.game_res_w = g_gfx_data->debug_gui.screenshot_width;
      options.game_res_h = g_gfx_data->debug_gui.screenshot_height;
      options.draw_region_width = options.game_res_w;
      options.draw_region_height = options.game_res_h;
      options.msaa_samples = g_gfx_data->debug_gui.screenshot_samples;
    }
    options.draw_small_profiler_window = g_gfx_data->debug_gui.small_profiler;
    options.pmode_alp_register = g_gfx_data->pmode_alp;

    GLint msaa_max;
    glGetIntegerv(GL_MAX_SAMPLES, &msaa_max);
    if (options.msaa_samples > msaa_max) {
      options.msaa_samples = msaa_max;
    }

    if (options.save_screenshot) {
      // ensure the screenshot has an extension
      std::string temp_path = g_gfx_data->debug_gui.screenshot_name();
      if (!endsWith(temp_path, ".png")) {
        temp_path += ".png";
      }
      options.screenshot_path = make_output_file_name(temp_path);
    }
    if constexpr (run_dma_copy) {
      auto& chain = g_gfx_data->dma_copier.get_last_result();
      g_gfx_data->ogl_renderer.render(DmaFollower(chain.data.data(), chain.start_offset), options);
    } else {
      auto p = scoped_prof("ogl-render");
      g_gfx_data->ogl_renderer.render(DmaFollower(g_gfx_data->dma_copier.get_last_input_data(),
                                                  g_gfx_data->dma_copier.get_last_input_offset()),
                                      options);
    }
  }

  // before vsync, mark the chain as rendered.
  {
    // should be fine to remove this mutex if the game actually waits for vsync to call
    // send_chain again. but let's be safe for now.
    std::unique_lock<std::mutex> lock(g_gfx_data->dma_mutex);
    g_gfx_data->engine_timer.start();
    g_gfx_data->has_data_to_render = false;
    g_gfx_data->sync_cv.notify_all();
  }
}

void GLDisplay::get_position(int* x, int* y) {
  SDL_GetWindowPosition(m_window, x, y);
}

void GLDisplay::get_size(int* width, int* height) {
  SDL_GL_GetDrawableSize(m_window, width, height);
}

void GLDisplay::get_scale(float* xs, float* ys) {
  // glfwGetWindowContentScale(m_window, xs, ys);
}

void GLDisplay::set_size(int width, int height) {
  SDL_SetWindowSize(m_window, width, height);
}

void GLDisplay::update_fullscreen(GfxDisplayMode mode, int /*screen*/) {
  //  GLFWmonitor* monitor = glfwGetPrimaryMonitor();  // todo
  //  switch (mode) {
  //    case GfxDisplayMode::Windowed: {
  //      // windowed
  //      glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_TRUE);
  //      glfwSetWindowFocusCallback(m_window, NULL);
  //      glfwSetWindowAttrib(m_window, GLFW_FLOATING, GLFW_FALSE);
  //      glfwSetWindowMonitor(m_window, NULL, xpos_backup(), ypos_backup(), width_backup(),
  //                           height_backup(), GLFW_DONT_CARE);
  //      set_imgui_visible(true);
  //    } break;
  //    case GfxDisplayMode::Fullscreen: {
  //      // fullscreen
  //      if (windowed()) {
  //        backup_params();
  //      }
  //      const GLFWvidmode* vmode = glfwGetVideoMode(monitor);
  //      glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_FALSE);
  //      glfwSetWindowFocusCallback(m_window, NULL);
  //      glfwSetWindowAttrib(m_window, GLFW_FLOATING, GLFW_FALSE);
  //      glfwSetWindowMonitor(m_window, monitor, 0, 0, vmode->width, vmode->height,
  //      GLFW_DONT_CARE); set_imgui_visible(false);
  //    } break;
  //    case GfxDisplayMode::Borderless: {
  //      // borderless fullscreen
  //      if (windowed()) {
  //        backup_params();
  //      }
  //      int x, y;
  //      glfwGetMonitorPos(monitor, &x, &y);
  //      const GLFWvidmode* vmode = glfwGetVideoMode(monitor);
  //      glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_FALSE);
  //      // glfwSetWindowAttrib(m_window, GLFW_FLOATING, GLFW_TRUE);
  //      // glfwSetWindowFocusCallback(m_window, FocusCallback);
  //#ifdef _WIN32
  //      glfwSetWindowMonitor(m_window, NULL, x, y, vmode->width, vmode->height + 1,
  //      GLFW_DONT_CARE);
  //#else
  //      glfwSetWindowMonitor(m_window, NULL, x, y, vmode->width, vmode->height, GLFW_DONT_CARE);
  //#endif
  //      set_imgui_visible(false);
  //    } break;
  //  }
}

GfxDisplayMode GLDisplay::get_fullscreen() {
  //  GLFWmonitor* monitor = glfwGetPrimaryMonitor();  // todo
  //  const GLFWvidmode* vmode = glfwGetVideoMode(monitor);
  //  if (glfwGetWindowMonitor(m_window) != NULL) {
  //    return GfxDisplayMode::Fullscreen;
  //  } else if (width() >= vmode->width && height() >= vmode->height) {
  //    return GfxDisplayMode::Borderless;
  //  } else {
  //    return GfxDisplayMode::Windowed;
  //  }
}

int GLDisplay::get_screen_vmode_count() {
  int count = 0;
  // glfwGetVideoModes(glfwGetPrimaryMonitor(), &count);
  return count;
}

void GLDisplay::get_screen_size(int vmode_idx, s32* w_out, s32* h_out) {
  //  auto vmode = glfwGetVideoMode(glfwGetPrimaryMonitor());
  //  int count = 0;
  //  auto vmodes = glfwGetVideoModes(glfwGetPrimaryMonitor(), &count);
  //  if (vmode_idx >= 0) {
  //    vmode = &vmodes[vmode_idx];
  //  } else if (get_fullscreen() == GfxDisplayMode::Fullscreen) {
  //    for (int i = 0; i < count; ++i) {
  //      if (!vmode || vmode->height < vmodes[i].height) {
  //        vmode = &vmodes[i];
  //      }
  //    }
  //  }
  //  if (w_out) {
  //    *w_out = vmode->width;
  //  }
  //  if (h_out) {
  //    *h_out = vmode->height;
  //  }
}

int GLDisplay::get_screen_rate(int vmode_idx) {
  //  auto vmode = glfwGetVideoMode(glfwGetPrimaryMonitor());
  //  int count = 0;
  //  auto vmodes = glfwGetVideoModes(glfwGetPrimaryMonitor(), &count);
  //  if (vmode_idx >= 0) {
  //    vmode = &vmodes[vmode_idx];
  //  } else if (get_fullscreen() == GfxDisplayMode::Fullscreen) {
  //    for (int i = 0; i < count; ++i) {
  //      if (!vmode || vmode->refreshRate < vmodes[i].refreshRate) {
  //        vmode = &vmodes[i];
  //      }
  //    }
  //  }
  //  return vmode->refreshRate;
  return 60;
}

bool GLDisplay::minimized() {
  u32 flags = SDL_GetWindowFlags(m_window);
  return flags & SDL_WINDOW_MINIMIZED;
}

void GLDisplay::set_lock(bool lock) {
  // glfwSetWindowAttrib(m_window, GLFW_RESIZABLE, lock ? GLFW_TRUE : GLFW_FALSE);
}

void update_global_profiler() {
  if (g_gfx_data->debug_gui.dump_events) {
    prof().set_enable(false);
    g_gfx_data->debug_gui.dump_events = false;
    prof().dump_to_json((file_util::get_jak_project_dir() / "prof.json").string());
  }
  prof().set_enable(g_gfx_data->debug_gui.record_events);
}

/*!
 * Main function called to render graphics frames. This is called in a loop.
 */
void GLDisplay::render() {
  // poll events
  {
    auto p = scoped_prof("poll-gamepads");
    Pad::update_gamepads();
  }

  // imgui start of frame
  {
    auto p = scoped_prof("imgui-init");
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
  }

  // framebuffer size
  int fbuf_w, fbuf_h;
  bool windows_borderless_hacks = false;
  SDL_GL_GetDrawableSize(m_window, &fbuf_w, &fbuf_h);
#ifdef _WIN32
  if (last_fullscreen_mode() == GfxDisplayMode::Borderless) {
    windows_borderless_hacks = true;
  }
#endif

  // render game!
  if (g_gfx_data->debug_gui.should_advance_frame()) {
    auto p = scoped_prof("game-render");
    int game_res_w = Gfx::g_global_settings.game_res_w;
    int game_res_h = Gfx::g_global_settings.game_res_h;
    if (game_res_w <= 0 || game_res_h <= 0) {
      // if the window size reports 0, the game will ask for a 0 sized window, and nothing likes
      // that.
      game_res_w = 640;
      game_res_h = 480;
    }
    render_game_frame(game_res_w, game_res_h, fbuf_w, fbuf_h, Gfx::g_global_settings.lbox_w,
                      Gfx::g_global_settings.lbox_h, Gfx::g_global_settings.msaa_samples,
                      windows_borderless_hacks);
  }

  // render debug
  if (is_imgui_visible()) {
    auto p = scoped_prof("debug-gui");
    g_gfx_data->debug_gui.draw(g_gfx_data->dma_copier.get_last_result().stats);
  }
  {
    auto p = scoped_prof("imgui-render");
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  }

  // actual vsync
  g_gfx_data->debug_gui.finish_frame();
  {
    auto p = scoped_prof("swap-buffers");
    SDL_GL_SwapWindow(m_window);
  }
  if (Gfx::g_global_settings.framelimiter) {
    auto p = scoped_prof("frame-limiter");
    g_gfx_data->frame_limiter.run(
        Gfx::g_global_settings.target_fps, Gfx::g_global_settings.experimental_accurate_lag,
        Gfx::g_global_settings.sleep_in_frame_limiter, g_gfx_data->last_engine_time);
  }
  // actually wait for vsync
  if (g_gfx_data->debug_gui.should_gl_finish()) {
    glFinish();
  }

  // switch vsync modes, if requested
  if (Gfx::g_global_settings.vsync != Gfx::g_global_settings.old_vsync) {
    Gfx::g_global_settings.old_vsync = Gfx::g_global_settings.vsync;
    SDL_GL_SetSwapInterval(Gfx::g_global_settings.vsync);
  }

  // Start timing for the next frame.
  g_gfx_data->debug_gui.start_frame();
  prof().instant_event("ROOT");
  update_global_profiler();

  // toggle even odd and wake up engine waiting on vsync.
  // TODO: we could play with moving this earlier, right after the final bucket renderer.
  //       it breaks the VIF-interrupt profiling though.
  {
    prof().instant_event("engine-notify");
    std::unique_lock<std::mutex> lock(g_gfx_data->sync_mutex);
    g_gfx_data->frame_idx++;
    g_gfx_data->sync_cv.notify_all();
  }

  {
    auto p = scoped_prof("fullscreen-update");
    // slow, takes ~0.15 ms on linux
    auto current_fullscreen_mode = get_fullscreen();
    // checking minimized also takes ~0.1 ms, only check if we need to update fullscreen modes
    if (current_fullscreen_mode != m_fullscreen_target_mode && !minimized()) {
      fullscreen_flush();
    }
    m_last_fullscreen_mode = current_fullscreen_mode;
  }

  // reboot whole game, if requested
  if (g_gfx_data->debug_gui.want_reboot_in_debug) {
    g_gfx_data->debug_gui.want_reboot_in_debug = false;
    MasterExit = RuntimeExitStatus::RESTART_IN_DEBUG;
  }

  {
    auto p = scoped_prof("check-close-window");
    // exit if display window was closed
    if (should_quit) {
      std::unique_lock<std::mutex> lock(g_gfx_data->sync_mutex);
      MasterExit = RuntimeExitStatus::EXIT;
      g_gfx_data->sync_cv.notify_all();
    }
  }
}

/*!
 * Wait for the next vsync. Returns 0 or 1 depending on if frame is even or odd.
 * Called from the game thread, on a GOAL stack.
 */
u32 gl_vsync() {
  if (!g_gfx_data) {
    return 0;
  }
  std::unique_lock<std::mutex> lock(g_gfx_data->sync_mutex);
  auto init_frame = g_gfx_data->frame_idx_of_input_data;
  g_gfx_data->sync_cv.wait(lock, [=] {
    return (MasterExit != RuntimeExitStatus::RUNNING) || g_gfx_data->frame_idx > init_frame;
  });
  return g_gfx_data->frame_idx & 1;
}

u32 gl_sync_path() {
  if (!g_gfx_data) {
    return 0;
  }
  std::unique_lock<std::mutex> lock(g_gfx_data->sync_mutex);
  g_gfx_data->last_engine_time = g_gfx_data->engine_timer.getSeconds();
  if (!g_gfx_data->has_data_to_render) {
    return 0;
  }
  g_gfx_data->sync_cv.wait(lock, [=] { return !g_gfx_data->has_data_to_render; });
  return 0;
}

/*!
 * Send DMA to the renderer.
 * Called from the game thread, on a GOAL stack.
 */
void gl_send_chain(const void* data, u32 offset) {
  if (g_gfx_data) {
    std::unique_lock<std::mutex> lock(g_gfx_data->dma_mutex);
    if (g_gfx_data->has_data_to_render) {
      lg::error(
          "Gfx::send_chain called when the graphics renderer has pending data. Was this called "
          "multiple times per frame?");
      return;
    }

    // we copy the dma data and give a copy of it to the render.
    // the copy has a few advantages:
    // - if the game code has a bug and corrupts the DMA buffer, the renderer won't see it.
    // - the copied DMA is much smaller than the entire game memory, so it can be dumped to a file
    //    separate of the entire RAM.
    // - it verifies the DMA data is valid early on.
    // but it may also be pretty expensive. Both the renderer and the game wait on this to complete.

    // The renderers should just operate on DMA chains, so eliminating this step in the future may
    // be easy.

    g_gfx_data->dma_copier.set_input_data(data, offset, run_dma_copy);

    g_gfx_data->has_data_to_render = true;
    g_gfx_data->dma_cv.notify_all();
  }
}

void gl_texture_upload_now(const u8* tpage, int mode, u32 s7_ptr) {
  // block
  if (g_gfx_data) {
    // just pass it to the texture pool.
    // the texture pool will take care of locking.
    // we don't want to lock here for the entire duration of the conversion.
    g_gfx_data->texture_pool->handle_upload_now(tpage, mode, g_ee_main_mem, s7_ptr);
  }
}

void gl_texture_relocate(u32 destination, u32 source, u32 format) {
  if (g_gfx_data) {
    g_gfx_data->texture_pool->relocate(destination, source, format);
  }
}

void gl_poll_events() {
  // glfwPollEvents();
}

void gl_set_levels(const std::vector<std::string>& levels) {
  g_gfx_data->loader->set_want_levels(levels);
}

void gl_set_pmode_alp(float val) {
  g_gfx_data->pmode_alp = val;
}

const GfxRendererModule gRendererOpenGL = {
    gl_init,                // init
    gl_make_display,        // make_display
    gl_exit,                // exit
    gl_vsync,               // vsync
    gl_sync_path,           // sync_path
    gl_send_chain,          // send_chain
    gl_texture_upload_now,  // texture_upload_now
    gl_texture_relocate,    // texture_relocate
    gl_poll_events,         // poll_events
    gl_set_levels,          // set_levels
    gl_set_pmode_alp,       // set_pmode_alp
    GfxPipeline::OpenGL,    // pipeline
    "OpenGL 4.3"            // name
};
