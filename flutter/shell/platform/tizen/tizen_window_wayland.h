// Copyright 2022 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_TIZEN_WINDOW_ECORE_WL2_H_
#define EMBEDDER_TIZEN_WINDOW_ECORE_WL2_H_

#include <glib.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wayland-egl.h>
#include <xkbcommon/xkbcommon.h>

#include <tizen-extension-client-protocol.h>
#include <xdg-shell-client-protocol.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "flutter/shell/platform/tizen/tizen_window.h"

namespace flutter {

class TizenWindowEcoreWl2 : public TizenWindow {
 public:
  TizenWindowEcoreWl2(TizenGeometry geometry,
                      bool transparent,
                      bool focusable,
                      bool top_level,
                      bool pointing_device_support,
                      bool floating_menu_support,
                      void* window_handle,
                      bool is_vulkan);

  ~TizenWindowEcoreWl2();

  TizenGeometry GetGeometry() override;

  bool SetGeometry(TizenGeometry geometry) override;

  TizenGeometry GetScreenGeometry() override;

  void* GetRenderTarget() override;

  void* GetRenderTargetDisplay() override { return wl2_display_; }

  void* GetNativeHandle() override { return wl2_surface_; }

  int32_t GetRotation() override;

  int32_t GetDpi() override;

  uintptr_t GetWindowId() override;

  uint32_t GetResourceId() override;

  void SetPreferredOrientations(const std::vector<int>& rotations) override;

  void BindKeys(const std::vector<std::string>& keys) override;

  void Show() override;

  void UpdateFlutterCursor(const std::string& kind) override;

  void ActivateWindow() override;

  void RaiseWindow() override;

  void LowerWindow() override;

  wl_display* GetDisplay() const { return wl2_display_; }
  wl_seat* GetSeat() const { return seat_; }
  wl_data_device_manager* GetDataDeviceManager() const {
    return data_device_manager_;
  }
  uint32_t GetLastInputSerial() const { return last_input_serial_; }

 private:
  struct KeyboardState {
    xkb_context* context = nullptr;
    xkb_keymap* keymap = nullptr;
    xkb_state* state = nullptr;

    xkb_mod_index_t shift_mod = XKB_MOD_INVALID;
    xkb_mod_index_t ctrl_mod = XKB_MOD_INVALID;
    xkb_mod_index_t alt_mod = XKB_MOD_INVALID;
    xkb_mod_index_t logo_mod = XKB_MOD_INVALID;

    xkb_led_index_t num_led = XKB_LED_INVALID;
    xkb_led_index_t caps_led = XKB_LED_INVALID;
    xkb_led_index_t scroll_led = XKB_LED_INVALID;

    uint32_t ecore_style_modifiers = 0;
  };

  bool CreateWindow(void* window_handle);

  void DestroyWindow();

  void SetWindowOptions();

  void EnableCursor();

#ifdef TV_PROFILE
  void SetPointingDeviceSupport();

  void SetFloatingMenuSupport();

  void ShowUnsupportedToast();
#endif

  void RegisterEventHandlers();

  void UnregisterEventHandlers();

  void SetTizenPolicyNotificationLevel(int level);

  void PrepareInputMethod();

  void UpdateKeyboardModifiers(uint32_t depressed,
                               uint32_t latched,
                               uint32_t locked,
                               uint32_t group);
  void UpdateOutputDpi();
  bool DispatchDisplayEvents();
  void SchedulePointerMotion(size_t timestamp);
  void FlushPendingPointerMotion();
  void CancelPendingPointerMotion();
  void UpdatePointerCursor();
  wl_cursor* ResolveCursorForKind(const std::string& kind) const;

  static gboolean HandleDisplayIO(GIOChannel* channel,
                                  GIOCondition condition,
                                  gpointer data);
  static gboolean DispatchPendingPointerMotion(gpointer data);

  static void HandleRegistryGlobal(void* data,
                                   wl_registry* registry,
                                   uint32_t name,
                                   const char* interface,
                                   uint32_t version);
  static void HandleRegistryGlobalRemove(void* data,
                                         wl_registry* registry,
                                         uint32_t name);

  static void HandleXdgWmBasePing(void* data,
                                  xdg_wm_base* wm_base,
                                  uint32_t serial);

  static void HandleXdgSurfaceConfigure(void* data,
                                        xdg_surface* surface,
                                        uint32_t serial);

  static void HandleXdgToplevelConfigure(void* data,
                                         xdg_toplevel* toplevel,
                                         int32_t width,
                                         int32_t height,
                                         wl_array* states);

  static void HandleXdgToplevelClose(void* data, xdg_toplevel* toplevel);

  static void HandleSeatCapabilities(void* data,
                                     wl_seat* seat,
                                     uint32_t capabilities);
  static void HandleSeatName(void* data, wl_seat* seat, const char* name);

  static void HandlePointerEnter(void* data,
                                 wl_pointer* pointer,
                                 uint32_t serial,
                                 wl_surface* surface,
                                 wl_fixed_t sx,
                                 wl_fixed_t sy);
  static void HandlePointerLeave(void* data,
                                 wl_pointer* pointer,
                                 uint32_t serial,
                                 wl_surface* surface);
  static void HandlePointerMotion(void* data,
                                  wl_pointer* pointer,
                                  uint32_t time,
                                  wl_fixed_t sx,
                                  wl_fixed_t sy);
  static void HandlePointerButton(void* data,
                                  wl_pointer* pointer,
                                  uint32_t serial,
                                  uint32_t time,
                                  uint32_t button,
                                  uint32_t state);
  static void HandlePointerAxis(void* data,
                                wl_pointer* pointer,
                                uint32_t time,
                                uint32_t axis,
                                wl_fixed_t value);

  static void HandleKeyboardKeymap(void* data,
                                   wl_keyboard* keyboard,
                                   uint32_t format,
                                   int32_t fd,
                                   uint32_t size);
  static void HandleKeyboardEnter(void* data,
                                  wl_keyboard* keyboard,
                                  uint32_t serial,
                                  wl_surface* surface,
                                  wl_array* keys);
  static void HandleKeyboardLeave(void* data,
                                  wl_keyboard* keyboard,
                                  uint32_t serial,
                                  wl_surface* surface);
  static void HandleKeyboardKey(void* data,
                                wl_keyboard* keyboard,
                                uint32_t serial,
                                uint32_t time,
                                uint32_t key,
                                uint32_t state);
  static void HandleKeyboardModifiers(void* data,
                                      wl_keyboard* keyboard,
                                      uint32_t serial,
                                      uint32_t depressed,
                                      uint32_t latched,
                                      uint32_t locked,
                                      uint32_t group);
  static void HandleKeyboardRepeatInfo(void* data,
                                       wl_keyboard* keyboard,
                                       int32_t rate,
                                       int32_t delay);

  static void HandleTouchDown(void* data,
                              wl_touch* touch,
                              uint32_t serial,
                              uint32_t time,
                              wl_surface* surface,
                              int32_t id,
                              wl_fixed_t x,
                              wl_fixed_t y);
  static void HandleTouchUp(void* data,
                            wl_touch* touch,
                            uint32_t serial,
                            uint32_t time,
                            int32_t id);
  static void HandleTouchMotion(void* data,
                                wl_touch* touch,
                                uint32_t time,
                                int32_t id,
                                wl_fixed_t x,
                                wl_fixed_t y);
  static void HandleTouchFrame(void* data, wl_touch* touch);
  static void HandleTouchCancel(void* data, wl_touch* touch);

  static void HandleOutputGeometry(void* data,
                                   wl_output* output,
                                   int32_t x,
                                   int32_t y,
                                   int32_t physical_width,
                                   int32_t physical_height,
                                   int32_t subpixel,
                                   const char* make,
                                   const char* model,
                                   int32_t transform);
  static void HandleOutputMode(void* data,
                               wl_output* output,
                               uint32_t flags,
                               int32_t width,
                               int32_t height,
                               int32_t refresh);
  static void HandleOutputDone(void* data, wl_output* output);
  static void HandleOutputScale(void* data, wl_output* output, int32_t factor);

  static void HandleResourceId(void* data,
                               tizen_resource* tizen_resource,
                               uint32_t id);

  wl_display* wl2_display_ = nullptr;
  wl_registry* registry_ = nullptr;
  wl_compositor* compositor_ = nullptr;
  wl_surface* wl2_surface_ = nullptr;
  wl_egl_window* wl_egl_window_ = nullptr;
  void* external_egl_window_ = nullptr;

  xdg_wm_base* xdg_wm_base_ = nullptr;
  xdg_surface* xdg_surface_ = nullptr;
  xdg_toplevel* xdg_toplevel_ = nullptr;
  wl_shell* wl_shell_ = nullptr;
  wl_shell_surface* wl_shell_surface_ = nullptr;

  wl_seat* seat_ = nullptr;
  wl_pointer* pointer_ = nullptr;
  wl_keyboard* keyboard_ = nullptr;
  wl_touch* touch_ = nullptr;
  wl_output* output_ = nullptr;
  wl_data_device_manager* data_device_manager_ = nullptr;
  wl_shm* shm_ = nullptr;
  wl_surface* cursor_surface_ = nullptr;
  wl_cursor_theme* cursor_theme_ = nullptr;
  wl_cursor* default_cursor_ = nullptr;

  tizen_policy* tizen_policy_ = nullptr;
  tizen_surface* tizen_surface_ = nullptr;
  tizen_indicator* tizen_indicator_ = nullptr;
  tizen_keyrouter* tizen_keyrouter_ = nullptr;
  tizen_screen_rotation* tizen_screen_rotation_ = nullptr;
  tizen_move_resize* tizen_move_resize_ = nullptr;
  tizen_position* tizen_position_ = nullptr;
  wl_text_input_manager* text_input_manager_ = nullptr;

  KeyboardState keyboard_state_;
  std::string seat_name_;
  std::unordered_map<int32_t, std::pair<double, double>> touch_points_;

  TizenGeometry geometry_ = {};
  TizenGeometry screen_geometry_ = {};
  int32_t output_transform_ = WL_OUTPUT_TRANSFORM_NORMAL;
  int32_t dpi_ = 0;
  int32_t rotation_degree_ = 0;
  int32_t output_physical_width_mm_ = 0;
  int32_t output_physical_height_mm_ = 0;

  double pointer_x_ = 0.0;
  double pointer_y_ = 0.0;
  bool pointer_button_pressed_ = false;
  bool pointer_inside_surface_ = false;
  uint32_t last_input_serial_ = 0;
  uint32_t pending_geometry_serial_ = 0;
  size_t pending_pointer_motion_timestamp_ = 0;
  std::string current_cursor_kind_ = "basic";

  bool running_ = false;
  bool owns_surface_ = true;
  bool is_vulkan_ = false;

  GIOChannel* display_io_channel_ = nullptr;
  guint display_io_watch_id_ = 0;
  guint pointer_motion_idle_id_ = 0;
  bool pointer_motion_pending_ = false;

  uint32_t resource_id_ = 0;

#ifdef TV_PROFILE
  bool pointing_device_support_ = true;
  bool floating_menu_support_ = true;
  bool show_unsupported_toast_ = false;
#endif
};

}  // namespace flutter

#endif  // EMBEDDER_TIZEN_WINDOW_ECORE_WL2_H_
