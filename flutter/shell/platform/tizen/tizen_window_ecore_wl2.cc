// Copyright 2022 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tizen_window_ecore_wl2.h"

#ifdef TV_PROFILE
#include <app.h>
#include <app_preference.h>
#include <time.h>
#include <vconf.h>
#endif

#include <linux/input.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include <text-client-protocol.h>
#include <wayland-client-protocol.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "flutter/shell/platform/embedder/embedder.h"
#include "flutter/shell/platform/tizen/logger.h"
#include "flutter/shell/platform/tizen/tizen_view_event_handler_delegate.h"

namespace flutter {

namespace {

// [TEMP_DIAG_REMOVE] Verbose runtime diagnostics for blank-screen triage.
#define TEMP_DIAG_ECORE_WL2(msg) do { } while (0)  // [TEMP_DIAG_REMOVE]

constexpr int kScrollDirectionVertical = WL_POINTER_AXIS_VERTICAL_SCROLL;
constexpr int kScrollDirectionHorizontal = WL_POINTER_AXIS_HORIZONTAL_SCROLL;

constexpr uint32_t kEcoreEventModifierShift = 0x0001;
constexpr uint32_t kEcoreEventModifierCtrl = 0x0002;
constexpr uint32_t kEcoreEventModifierAlt = 0x0004;
constexpr uint32_t kEcoreEventModifierWin = 0x0008;
constexpr uint32_t kEcoreEventModifierScroll = 0x0010;
constexpr uint32_t kEcoreEventModifierNum = 0x0020;
constexpr uint32_t kEcoreEventModifierCaps = 0x0040;

constexpr uint32_t kEcoreDeviceClassKeyboard = 2;
constexpr uint32_t kEcoreDeviceSubclassNone = 0;

int32_t ToRotationDegree(int32_t transform) {
  switch (transform) {
    case WL_OUTPUT_TRANSFORM_90:
    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
      return 90;
    case WL_OUTPUT_TRANSFORM_180:
    case WL_OUTPUT_TRANSFORM_FLIPPED_180:
      return 180;
    case WL_OUTPUT_TRANSFORM_270:
    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
      return 270;
    case WL_OUTPUT_TRANSFORM_NORMAL:
    case WL_OUTPUT_TRANSFORM_FLIPPED:
    default:
      return 0;
  }
}

FlutterPointerMouseButtons ToFlutterPointerButton(uint32_t button) {
  switch (button) {
    case BTN_MIDDLE:
      return kFlutterPointerButtonMouseMiddle;
    case BTN_RIGHT:
      return kFlutterPointerButtonMouseSecondary;
    case BTN_LEFT:
    default:
      return kFlutterPointerButtonMousePrimary;
  }
}

xkb_keysym_t ResolveKeySymbolAlias(const std::string& key) {
  if (key == "XF86PlayBack") {
    return XKB_KEY_XF86AudioPlay;
  }
  if (key == "XF86ChannelGuide") {
    return XKB_KEY_XF86Guide;
  }
  if (key == "XF86Caption") {
    return XKB_KEY_XF86Subtitle;
  }
  if (key == "XF86Exit") {
    return XKB_KEY_XF86Close;
  }
  return XKB_KEY_NoSymbol;
}

}  // namespace

TizenWindowEcoreWl2::TizenWindowEcoreWl2(TizenGeometry geometry,
                                         bool transparent,
                                         bool focusable,
                                         bool top_level,
                                         bool pointing_device_support,
                                         bool floating_menu_support,
                                         void* window_handle,
                                         bool is_vulkan)
    : TizenWindow(geometry, transparent, focusable, top_level),
      geometry_(geometry),
      is_vulkan_(is_vulkan)
#ifdef TV_PROFILE
      ,
      pointing_device_support_(pointing_device_support),
      floating_menu_support_(floating_menu_support)
#endif
{
  if (!CreateWindow(window_handle)) {
    FT_LOG(Error) << "Failed to create a platform window.";
    return;
  }

  SetWindowOptions();
  RegisterEventHandlers();
  PrepareInputMethod();
  Show();
}

TizenWindowEcoreWl2::~TizenWindowEcoreWl2() {
  input_method_context_.reset();
  UnregisterEventHandlers();
  DestroyWindow();
}

bool TizenWindowEcoreWl2::CreateWindow(void* window_handle) {
  TEMP_DIAG_ECORE_WL2("CreateWindow begin [diag-r7]. window_handle=" << window_handle);
  wl2_display_ = wl_display_connect(nullptr);

  if (!wl2_display_) {
    FT_LOG(Error) << "Could not connect to Wayland display.";
    return false;
  }

  registry_ = wl_display_get_registry(wl2_display_);
  if (!registry_) {
    FT_LOG(Error) << "Could not get Wayland registry.";
    return false;
  }

  static const wl_registry_listener kRegistryListener = {
      HandleRegistryGlobal,
      HandleRegistryGlobalRemove,
  };
  wl_registry_add_listener(registry_, &kRegistryListener, this);

  wl_display_roundtrip(wl2_display_);
  wl_display_roundtrip(wl2_display_);
  TEMP_DIAG_ECORE_WL2("Registry roundtrip done. compositor=" << compositor_
                      << " xdg_wm_base=" << xdg_wm_base_
                      << " seat=" << seat_ << " output=" << output_);

  if (!window_handle && !compositor_) {
    FT_LOG(Error) << "Missing required Wayland globals: wl_compositor.";
    return false;
  }

  if (window_handle) {
    // Legacy hosts may still pass an Ecore window handle here. Without Ecore,
    // we cannot safely unwrap that to wl_surface. Prefer creating our own
    // wl_surface when compositor is available.
    if (compositor_) {
      wl2_surface_ = wl_compositor_create_surface(compositor_);
      owns_surface_ = true;
      FT_LOG(Info) << "Ignored legacy pre-created window handle; created wl_surface via compositor.";
    } else {
      wl2_surface_ = static_cast<wl_surface*>(window_handle);
      owns_surface_ = false;
      FT_LOG(Info) << "Using pre-created window handle as wl_surface.";
    }
  } else {
    wl2_surface_ = wl_compositor_create_surface(compositor_);
    owns_surface_ = true;
  }

  if (!wl2_surface_) {
    FT_LOG(Error) << "Could not create Wayland surface.";
    return false;
  }
  TEMP_DIAG_ECORE_WL2("Surface ready. wl_surface=" << wl2_surface_
                      << " owns_surface=" << owns_surface_);

  if (xdg_wm_base_) {
    xdg_surface_ = xdg_wm_base_get_xdg_surface(xdg_wm_base_, wl2_surface_);
    if (!xdg_surface_) {
      FT_LOG(Error) << "Could not create xdg_surface.";
      return false;
    }

    static const xdg_surface_listener kXdgSurfaceListener = {
        HandleXdgSurfaceConfigure,
    };
    xdg_surface_add_listener(xdg_surface_, &kXdgSurfaceListener, this);

    xdg_toplevel_ = xdg_surface_get_toplevel(xdg_surface_);
    if (!xdg_toplevel_) {
      FT_LOG(Error) << "Could not create xdg_toplevel.";
      return false;
    }

    static const xdg_toplevel_listener kXdgToplevelListener = {
        HandleXdgToplevelConfigure,
        HandleXdgToplevelClose,
    };
    xdg_toplevel_add_listener(xdg_toplevel_, &kXdgToplevelListener, this);

    static const xdg_wm_base_listener kWmBaseListener = {
        HandleXdgWmBasePing,
    };
    xdg_wm_base_add_listener(xdg_wm_base_, &kWmBaseListener, this);
    TEMP_DIAG_ECORE_WL2("Using xdg-shell path.");
  } else {
    TEMP_DIAG_ECORE_WL2("xdg_wm_base not available. Trying wl_shell fallback.");
    if (wl_shell_) {
      wl_shell_surface_ = wl_shell_get_shell_surface(wl_shell_, wl2_surface_);
      if (wl_shell_surface_) {
        wl_shell_surface_set_toplevel(wl_shell_surface_);
        TEMP_DIAG_ECORE_WL2("wl_shell fallback active. shell_surface="
                            << wl_shell_surface_);
      } else {
        TEMP_DIAG_ECORE_WL2("wl_shell fallback failed to create shell_surface.");
      }
    } else {
      TEMP_DIAG_ECORE_WL2("No wl_shell global available either.");
    }

    if (window_handle) {
      // In some hosts, window_handle is already an EGL-native window object.
      // Keep it as a render target fallback when wl_egl_window creation is
      // not possible or does not present frames.
      external_egl_window_ = window_handle;
    }
  }

  if (screen_geometry_.width <= 0 || screen_geometry_.height <= 0) {
    screen_geometry_.width = 720;
    screen_geometry_.height = 1280;
  }

  if (geometry_.width <= 0) {
    geometry_.width = screen_geometry_.width;
  }
  if (geometry_.height <= 0) {
    geometry_.height = screen_geometry_.height;
  }

  if (!is_vulkan_) {
    wl_egl_window_ =
        wl_egl_window_create(wl2_surface_, geometry_.width, geometry_.height);
    TEMP_DIAG_ECORE_WL2("EGL window create result. wl_egl_window="
                        << wl_egl_window_ << " external_egl_window="
                        << external_egl_window_ << " geometry="
                        << geometry_.width << "x" << geometry_.height);
    if (!wl_egl_window_ && !external_egl_window_) {
      FT_LOG(Error) << "Could not create wl_egl_window.";
      return false;
    }
    if (!wl_egl_window_ && external_egl_window_) {
      FT_LOG(Info) << "Falling back to external EGL window handle.";
    }
  }

  if (xdg_wm_base_ || owns_surface_) {
    wl_surface_commit(wl2_surface_);
  }
  wl_display_flush(wl2_display_);

  int display_fd = wl_display_get_fd(wl2_display_);
  if (display_fd >= 0) {
    display_io_channel_ = g_io_channel_unix_new(display_fd);
    if (display_io_channel_) {
      GError* error = nullptr;
      g_io_channel_set_encoding(display_io_channel_, nullptr, &error);
      if (error) {
        FT_LOG(Error) << "Failed to set GIOChannel encoding: "
                      << error->message;
        g_error_free(error);
      }
      g_io_channel_set_buffered(display_io_channel_, FALSE);
      display_io_watch_id_ = g_io_add_watch(
          display_io_channel_,
          static_cast<GIOCondition>(G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL),
          HandleDisplayIO, this);
    }
  }

  running_ = true;
  TEMP_DIAG_ECORE_WL2("CreateWindow success. running=" << running_);
  return true;
}

void TizenWindowEcoreWl2::SetWindowOptions() {
  if (top_level_) {
    SetTizenPolicyNotificationLevel(TIZEN_POLICY_LEVEL_TOP);
  }

  if (tizen_policy_ && wl2_surface_) {
    tizen_policy_set_type(
        tizen_policy_, wl2_surface_,
        top_level_ ? TIZEN_POLICY_WIN_TYPE_NOTIFICATION
                   : TIZEN_POLICY_WIN_TYPE_TOPLEVEL);

    if (!focusable_) {
      tizen_policy_set_focus_skip(tizen_policy_, wl2_surface_);
    }
  }

  if (tizen_indicator_ && wl2_surface_) {
    tizen_indicator_set_state(tizen_indicator_, wl2_surface_,
                              TIZEN_INDICATOR_STATE_ON);
    tizen_indicator_set_opacity_mode(tizen_indicator_, wl2_surface_,
                                     TIZEN_INDICATOR_OPACITY_MODE_OPAQUE);
    tizen_indicator_set_visible_type(tizen_indicator_, wl2_surface_,
                                     TIZEN_INDICATOR_VISIBLE_TYPE_SHOWN);
  }

  if (tizen_position_) {
    tizen_position_set(tizen_position_, initial_geometry_.left,
                       initial_geometry_.top);
  }

  EnableCursor();
}

void TizenWindowEcoreWl2::EnableCursor() {
  // [TEMP_DIAG_REMOVE] Cursor restore path (wl_shm-backed).
  if (!compositor_ || !wl2_display_ || !shm_) {
    TEMP_DIAG_ECORE_WL2("EnableCursor skipped. compositor=" << compositor_
                        << " display=" << wl2_display_ << " shm=" << shm_);
    return;
  }

  if (!cursor_surface_) {
    cursor_surface_ = wl_compositor_create_surface(compositor_);
  }
  if (!cursor_theme_) {
    cursor_theme_ = wl_cursor_theme_load(nullptr, 24, shm_);
  }
  if (cursor_theme_ && !default_cursor_) {
    default_cursor_ = wl_cursor_theme_get_cursor(cursor_theme_, "left_ptr");
  }

  TEMP_DIAG_ECORE_WL2("EnableCursor ready. cursor_surface=" << cursor_surface_
                      << " theme=" << cursor_theme_
                      << " cursor=" << default_cursor_);
}

#ifdef TV_PROFILE
typedef enum _MouseSupport { DISABLE = 0, ENABLE } MouseSupport;
typedef enum _Device_Type { MOUSE_DEVICE = 3, TOUCH_DEVICE } Device_Type;

void TizenWindowEcoreWl2::SetPointingDeviceSupport() {
  FT_LOG(Info) << "SetPointingDeviceSupport is not available without Ecore.";
}

void TizenWindowEcoreWl2::SetFloatingMenuSupport() {
  FT_LOG(Info) << "SetFloatingMenuSupport is not available without Ecore.";
}

void TizenWindowEcoreWl2::ShowUnsupportedToast() {
  FT_LOG(Info) << "ShowUnsupportedToast is not available without Ecore.";
}
#endif

void TizenWindowEcoreWl2::RegisterEventHandlers() {
  if (tizen_screen_rotation_ && wl2_surface_) {
    tizen_screen_rotation_get_ignore_output_transform(tizen_screen_rotation_,
                                                      wl2_surface_);
  }
}

void TizenWindowEcoreWl2::UnregisterEventHandlers() {
  if (display_io_watch_id_ != 0) {
    g_source_remove(display_io_watch_id_);
    display_io_watch_id_ = 0;
  }

  if (display_dispatch_source_id_ != 0) {
    g_source_remove(display_dispatch_source_id_);
    display_dispatch_source_id_ = 0;
    display_io_pending_ = false;
  }

  if (display_io_channel_) {
    g_io_channel_unref(display_io_channel_);
    display_io_channel_ = nullptr;
  }
}

void TizenWindowEcoreWl2::DestroyWindow() {
  running_ = false;

  if (pointer_) {
    wl_pointer_destroy(pointer_);
    pointer_ = nullptr;
  }

  if (keyboard_) {
    wl_keyboard_destroy(keyboard_);
    keyboard_ = nullptr;
  }

  if (touch_) {
    wl_touch_destroy(touch_);
    touch_ = nullptr;
  }

  if (seat_) {
    wl_seat_destroy(seat_);
    seat_ = nullptr;
  }

  if (cursor_surface_) {
    wl_surface_destroy(cursor_surface_);
    cursor_surface_ = nullptr;
  }

  if (cursor_theme_) {
    wl_cursor_theme_destroy(cursor_theme_);
    cursor_theme_ = nullptr;
    default_cursor_ = nullptr;
  }

  if (output_) {
    wl_output_destroy(output_);
    output_ = nullptr;
  }

  if (tizen_keyrouter_) {
    tizen_keyrouter_destroy(tizen_keyrouter_);
    tizen_keyrouter_ = nullptr;
  }

  if (tizen_indicator_) {
    tizen_indicator_destroy(tizen_indicator_);
    tizen_indicator_ = nullptr;
  }

  if (tizen_position_) {
    tizen_position_destroy(tizen_position_);
    tizen_position_ = nullptr;
  }

  if (tizen_move_resize_) {
    tizen_move_resize_destroy(tizen_move_resize_);
    tizen_move_resize_ = nullptr;
  }

  if (tizen_screen_rotation_) {
    tizen_screen_rotation_destroy(tizen_screen_rotation_);
    tizen_screen_rotation_ = nullptr;
  }

  if (tizen_policy_) {
    tizen_policy_destroy(tizen_policy_);
    tizen_policy_ = nullptr;
  }

  if (tizen_surface_) {
    tizen_surface_destroy(tizen_surface_);
    tizen_surface_ = nullptr;
  }

  if (text_input_manager_) {
    wl_text_input_manager_destroy(text_input_manager_);
    text_input_manager_ = nullptr;
  }

  if (data_device_manager_) {
    wl_data_device_manager_destroy(data_device_manager_);
    data_device_manager_ = nullptr;
  }

  if (shm_) {
    wl_shm_destroy(shm_);
    shm_ = nullptr;
  }

  if (xdg_toplevel_) {
    xdg_toplevel_destroy(xdg_toplevel_);
    xdg_toplevel_ = nullptr;
  }

  if (xdg_surface_) {
    xdg_surface_destroy(xdg_surface_);
    xdg_surface_ = nullptr;
  }

  if (xdg_wm_base_) {
    xdg_wm_base_destroy(xdg_wm_base_);
    xdg_wm_base_ = nullptr;
  }

  if (wl_shell_surface_) {
    wl_shell_surface_destroy(wl_shell_surface_);
    wl_shell_surface_ = nullptr;
  }

  if (wl_shell_) {
    wl_shell_destroy(wl_shell_);
    wl_shell_ = nullptr;
  }

  if (wl_egl_window_) {
    wl_egl_window_destroy(wl_egl_window_);
    wl_egl_window_ = nullptr;
  }

  if (wl2_surface_ && owns_surface_) {
    wl_surface_destroy(wl2_surface_);
  }
  wl2_surface_ = nullptr;

  if (compositor_) {
    wl_compositor_destroy(compositor_);
    compositor_ = nullptr;
  }

  if (registry_) {
    wl_registry_destroy(registry_);
    registry_ = nullptr;
  }

  if (wl2_display_) {
    wl_display_disconnect(wl2_display_);
    wl2_display_ = nullptr;
  }

  if (keyboard_state_.state) {
    xkb_state_unref(keyboard_state_.state);
    keyboard_state_.state = nullptr;
  }

  if (keyboard_state_.keymap) {
    xkb_keymap_unref(keyboard_state_.keymap);
    keyboard_state_.keymap = nullptr;
  }

  if (keyboard_state_.context) {
    xkb_context_unref(keyboard_state_.context);
    keyboard_state_.context = nullptr;
  }
}

TizenGeometry TizenWindowEcoreWl2::GetGeometry() {
  return geometry_;
}

bool TizenWindowEcoreWl2::SetGeometry(TizenGeometry geometry) {
  geometry_ = geometry;

  if (wl_egl_window_) {
    wl_egl_window_resize(wl_egl_window_, geometry_.width, geometry_.height,
                         0, 0);
  }

  if (tizen_move_resize_ && wl2_surface_) {
    tizen_move_resize_set_geometry(
        tizen_move_resize_, wl2_surface_, pending_geometry_serial_,
        geometry_.left, geometry_.top, geometry_.width, geometry_.height);
  }

  if (tizen_position_) {
    tizen_position_set(tizen_position_, geometry_.left, geometry_.top);
  }

  if (wl2_surface_) {
    wl_surface_commit(wl2_surface_);
  }
  if (wl2_display_) {
    wl_display_flush(wl2_display_);
  }

  return true;
}

TizenGeometry TizenWindowEcoreWl2::GetScreenGeometry() {
  if (screen_geometry_.width <= 0 || screen_geometry_.height <= 0) {
    return geometry_;
  }
  return screen_geometry_;
}

int32_t TizenWindowEcoreWl2::GetRotation() {
  return rotation_degree_;
}

int32_t TizenWindowEcoreWl2::GetDpi() {
  return dpi_;
}

uintptr_t TizenWindowEcoreWl2::GetWindowId() {
  uint32_t resource_id = GetResourceId();
  if (resource_id > 0) {
    return resource_id;
  }
  return reinterpret_cast<uintptr_t>(wl2_surface_);
}

uint32_t TizenWindowEcoreWl2::GetResourceId() {
  if (resource_id_ > 0) {
    return resource_id_;
  }

  if (!tizen_surface_ || !wl2_surface_) {
    return 0;
  }

  tizen_resource* resource =
      tizen_surface_get_tizen_resource(tizen_surface_, wl2_surface_);
  if (!resource) {
    FT_LOG(Error) << "Failed to get tizen resource.";
    return 0;
  }

  static const tizen_resource_listener kResourceListener = {
      HandleResourceId,
  };

  tizen_resource_add_listener(resource, &kResourceListener, this);
  wl_display_roundtrip(wl2_display_);
  tizen_resource_destroy(resource);

  return resource_id_;
}

void TizenWindowEcoreWl2::SetPreferredOrientations(
    const std::vector<int>& rotations) {
  FT_LOG(Info)
      << "SetPreferredOrientations is not implemented by direct Wayland API.";
}

void TizenWindowEcoreWl2::BindKeys(const std::vector<std::string>& keys) {
  if (!tizen_keyrouter_ || !wl2_surface_) {
    return;
  }

  for (const std::string& key : keys) {
    uint32_t keycode = static_cast<uint32_t>(
        xkb_keysym_from_name(key.c_str(), XKB_KEYSYM_NO_FLAGS));
    if (keycode == XKB_KEY_NoSymbol) {
      keycode = static_cast<uint32_t>(ResolveKeySymbolAlias(key));
    }
    if (keycode == XKB_KEY_NoSymbol) {
      char* end = nullptr;
      unsigned long parsed = std::strtoul(key.c_str(), &end, 0);
      if (end && *end == '\0') {
        keycode = static_cast<uint32_t>(parsed);
      } else {
        FT_LOG(Error) << "Failed to parse key grab symbol: " << key;
        continue;
      }
    }

    tizen_keyrouter_set_keygrab(tizen_keyrouter_, wl2_surface_, keycode,
                                TIZEN_KEYROUTER_MODE_TOPMOST);
  }

  wl_display_flush(wl2_display_);
}

void TizenWindowEcoreWl2::Show() {
  TEMP_DIAG_ECORE_WL2("Show called. wl_surface=" << wl2_surface_
                      << " xdg=" << xdg_wm_base_);
  if (!wl2_surface_) {
    return;
  }

  if (xdg_wm_base_ || owns_surface_) {
    wl_surface_commit(wl2_surface_);
  }
  wl_display_flush(wl2_display_);
}

void TizenWindowEcoreWl2::UpdateFlutterCursor(const std::string& kind) {
  FT_LOG(Info) << "UpdateFlutterCursor is not supported without Ecore: "
               << kind;
}

void TizenWindowEcoreWl2::SetTizenPolicyNotificationLevel(int level) {
  if (tizen_policy_ && wl2_surface_) {
    tizen_policy_set_notification_level(tizen_policy_, wl2_surface_, level);
  }
}

void TizenWindowEcoreWl2::PrepareInputMethod() {
  input_method_context_ = std::make_unique<TizenInputMethodContext>(
      GetWindowId(), wl2_display_, seat_, wl2_surface_, text_input_manager_);

  input_method_context_->SetOnPreeditStart([this]() {
    if (view_delegate_) {
      view_delegate_->OnComposeBegin();
    }
  });
  input_method_context_->SetOnPreeditChanged(
      [this](std::string str, int cursor_pos) {
        if (view_delegate_) {
          view_delegate_->OnComposeChange(str, cursor_pos);
        }
      });
  input_method_context_->SetOnPreeditEnd([this]() {
    if (view_delegate_) {
      view_delegate_->OnComposeEnd();
    }
  });
  input_method_context_->SetOnCommit([this](std::string str) {
    if (view_delegate_) {
      view_delegate_->OnCommit(str);
    }
  });
}

void* TizenWindowEcoreWl2::GetRenderTarget() {
  if (is_vulkan_) {
    return wl2_surface_;
  }
  if (wl_egl_window_) {
    return wl_egl_window_;
  }
  return external_egl_window_;
}

void TizenWindowEcoreWl2::ActivateWindow() {
  if (tizen_policy_ && wl2_surface_) {
    tizen_policy_activate(tizen_policy_, wl2_surface_);
    wl_display_flush(wl2_display_);
  }
}

void TizenWindowEcoreWl2::RaiseWindow() {
  if (tizen_policy_ && wl2_surface_) {
    tizen_policy_raise(tizen_policy_, wl2_surface_);
    wl_display_flush(wl2_display_);
  }
}

void TizenWindowEcoreWl2::LowerWindow() {
  if (tizen_policy_ && wl2_surface_) {
    tizen_policy_lower(tizen_policy_, wl2_surface_);
    wl_display_flush(wl2_display_);
  }
}

void TizenWindowEcoreWl2::UpdateKeyboardModifiers(uint32_t depressed,
                                                  uint32_t latched,
                                                  uint32_t locked,
                                                  uint32_t group) {
  if (!keyboard_state_.state) {
    return;
  }

  xkb_state_update_mask(keyboard_state_.state, depressed, latched, locked, 0,
                        0, group);

  uint32_t modifiers = 0;
  const xkb_state_component kAnyComponent = static_cast<xkb_state_component>(
      XKB_STATE_MODS_DEPRESSED | XKB_STATE_MODS_LATCHED | XKB_STATE_MODS_LOCKED);

  if (keyboard_state_.shift_mod != XKB_MOD_INVALID &&
      xkb_state_mod_index_is_active(keyboard_state_.state,
                                    keyboard_state_.shift_mod,
                                    kAnyComponent)) {
    modifiers |= kEcoreEventModifierShift;
  }

  if (keyboard_state_.ctrl_mod != XKB_MOD_INVALID &&
      xkb_state_mod_index_is_active(keyboard_state_.state,
                                    keyboard_state_.ctrl_mod,
                                    kAnyComponent)) {
    modifiers |= kEcoreEventModifierCtrl;
  }

  if (keyboard_state_.alt_mod != XKB_MOD_INVALID &&
      xkb_state_mod_index_is_active(keyboard_state_.state, keyboard_state_.alt_mod,
                                    kAnyComponent)) {
    modifiers |= kEcoreEventModifierAlt;
  }

  if (keyboard_state_.logo_mod != XKB_MOD_INVALID &&
      xkb_state_mod_index_is_active(keyboard_state_.state,
                                    keyboard_state_.logo_mod,
                                    kAnyComponent)) {
    modifiers |= kEcoreEventModifierWin;
  }

  if (keyboard_state_.num_led != XKB_LED_INVALID &&
      xkb_state_led_index_is_active(keyboard_state_.state,
                                    keyboard_state_.num_led)) {
    modifiers |= kEcoreEventModifierNum;
  }

  if (keyboard_state_.caps_led != XKB_LED_INVALID &&
      xkb_state_led_index_is_active(keyboard_state_.state,
                                    keyboard_state_.caps_led)) {
    modifiers |= kEcoreEventModifierCaps;
  }

  if (keyboard_state_.scroll_led != XKB_LED_INVALID &&
      xkb_state_led_index_is_active(keyboard_state_.state,
                                    keyboard_state_.scroll_led)) {
    modifiers |= kEcoreEventModifierScroll;
  }

  keyboard_state_.ecore_style_modifiers = modifiers;
}

void TizenWindowEcoreWl2::UpdateOutputDpi() {
  if (screen_geometry_.width <= 0 || screen_geometry_.height <= 0 ||
      output_physical_width_mm_ <= 0 || output_physical_height_mm_ <= 0) {
    if (dpi_ <= 0) {
      dpi_ = 160;
    }
    return;
  }

  int32_t xdpi = static_cast<int32_t>(
      (screen_geometry_.width * 25.4) / output_physical_width_mm_);
  int32_t ydpi = static_cast<int32_t>(
      (screen_geometry_.height * 25.4) / output_physical_height_mm_);

  if (xdpi > 0 && ydpi > 0) {
    dpi_ = (xdpi + ydpi) / 2;
  } else if (xdpi > 0) {
    dpi_ = xdpi;
  } else if (ydpi > 0) {
    dpi_ = ydpi;
  } else if (dpi_ <= 0) {
    dpi_ = 160;
  }
}

gboolean TizenWindowEcoreWl2::HandleDisplayIO(GIOChannel* channel,
                                              GIOCondition condition,
                                              gpointer data) {
  auto* self = static_cast<TizenWindowEcoreWl2*>(data);
  if (!self || !self->running_ || !self->wl2_display_) {
    return FALSE;
  }

  if (condition & (G_IO_HUP | G_IO_ERR | G_IO_NVAL)) {
    FT_LOG(Error) << "Wayland display watch got an error condition.";
    return FALSE;
  }

  if (condition & G_IO_IN) {
    self->display_io_pending_ = true;
    if (self->display_dispatch_source_id_ == 0) {
      constexpr guint kDispatchIntervalMs = 16;
      self->display_dispatch_source_id_ = g_timeout_add_full(
          G_PRIORITY_DEFAULT, kDispatchIntervalMs, DispatchDisplayIO, self,
          nullptr);
    }
  }

  return TRUE;
}

gboolean TizenWindowEcoreWl2::DispatchDisplayIO(gpointer data) {
  auto* self = static_cast<TizenWindowEcoreWl2*>(data);
  if (!self) {
    return G_SOURCE_REMOVE;
  }

  self->display_dispatch_source_id_ = 0;

  if (!self->running_ || !self->wl2_display_ || !self->display_io_pending_) {
    self->display_io_pending_ = false;
    return G_SOURCE_REMOVE;
  }

  self->display_io_pending_ = false;

  if (wl_display_dispatch(self->wl2_display_) < 0) {
    FT_LOG(Error) << "wl_display_dispatch failed.";
    return G_SOURCE_REMOVE;
  }

  for (int i = 0; i < 8; ++i) {
    if (wl_display_dispatch_pending(self->wl2_display_) <= 0) {
      break;
    }
  }

  return G_SOURCE_REMOVE;
}

void TizenWindowEcoreWl2::HandleRegistryGlobal(void* data,
                                               wl_registry* registry,
                                               uint32_t name,
                                               const char* interface,
                                               uint32_t version) {
  auto* self = static_cast<TizenWindowEcoreWl2*>(data);
  if (!self) {
    return;
  }

  if (strcmp(interface, wl_compositor_interface.name) == 0) {
    self->compositor_ = static_cast<wl_compositor*>(
        wl_registry_bind(registry, name, &wl_compositor_interface,
                         std::min(version, 4u)));
    TEMP_DIAG_ECORE_WL2("Bind wl_compositor name=" << name << " ver=" << version);
  } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
    self->xdg_wm_base_ = static_cast<xdg_wm_base*>(
        wl_registry_bind(registry, name, &xdg_wm_base_interface,
                         std::min(version, 1u)));
    TEMP_DIAG_ECORE_WL2("Bind xdg_wm_base name=" << name << " ver=" << version);
  } else if (strcmp(interface, wl_shell_interface.name) == 0) {
    self->wl_shell_ = static_cast<wl_shell*>(
        wl_registry_bind(registry, name, &wl_shell_interface, 1));
    TEMP_DIAG_ECORE_WL2("Bind wl_shell name=" << name << " ver=" << version);
  } else if (strcmp(interface, wl_seat_interface.name) == 0) {
    self->seat_ = static_cast<wl_seat*>(
        wl_registry_bind(registry, name, &wl_seat_interface,
                         std::min(version, 5u)));
    TEMP_DIAG_ECORE_WL2("Bind wl_seat name=" << name << " ver=" << version);
    static const wl_seat_listener kSeatListener = {
        HandleSeatCapabilities,
        HandleSeatName,
    };
    wl_seat_add_listener(self->seat_, &kSeatListener, self);
  } else if (strcmp(interface, wl_output_interface.name) == 0) {
    if (!self->output_) {
      TEMP_DIAG_ECORE_WL2("Bind wl_output name=" << name << " ver=" << version);
      self->output_ = static_cast<wl_output*>(
          wl_registry_bind(registry, name, &wl_output_interface,
                           std::min(version, 3u)));
      static const wl_output_listener kOutputListener = {
          HandleOutputGeometry,
          HandleOutputMode,
          HandleOutputDone,
          HandleOutputScale,
      };
      wl_output_add_listener(self->output_, &kOutputListener, self);
    }
  } else if (strcmp(interface, wl_data_device_manager_interface.name) == 0) {
    self->data_device_manager_ = static_cast<wl_data_device_manager*>(
        wl_registry_bind(registry, name, &wl_data_device_manager_interface,
                         std::min(version, 3u)));
  } else if (strcmp(interface, wl_shm_interface.name) == 0) {
    self->shm_ = static_cast<wl_shm*>(
        wl_registry_bind(registry, name, &wl_shm_interface, std::min(version, 1u)));
    TEMP_DIAG_ECORE_WL2("Bind wl_shm name=" << name << " ver=" << version);
  } else if (strcmp(interface, tizen_policy_interface.name) == 0) {
    self->tizen_policy_ = static_cast<tizen_policy*>(
        wl_registry_bind(registry, name, &tizen_policy_interface,
                         std::min(version, 15u)));
    TEMP_DIAG_ECORE_WL2("Bind tizen_policy name=" << name << " ver=" << version);
  } else if (strcmp(interface, tizen_indicator_interface.name) == 0) {
    self->tizen_indicator_ = static_cast<tizen_indicator*>(
        wl_registry_bind(registry, name, &tizen_indicator_interface,
                         std::min(version, 1u)));
  } else if (strcmp(interface, tizen_keyrouter_interface.name) == 0) {
    self->tizen_keyrouter_ = static_cast<tizen_keyrouter*>(
        wl_registry_bind(registry, name, &tizen_keyrouter_interface,
                         std::min(version, 2u)));
    TEMP_DIAG_ECORE_WL2("Bind tizen_keyrouter name=" << name << " ver=" << version);
  } else if (strcmp(interface, tizen_surface_interface.name) == 0) {
    self->tizen_surface_ = static_cast<tizen_surface*>(
        wl_registry_bind(registry, name, &tizen_surface_interface,
                         std::min(version, 1u)));
    TEMP_DIAG_ECORE_WL2("Bind tizen_surface name=" << name << " ver=" << version);
  } else if (strcmp(interface, tizen_screen_rotation_interface.name) == 0) {
    self->tizen_screen_rotation_ = static_cast<tizen_screen_rotation*>(
        wl_registry_bind(registry, name, &tizen_screen_rotation_interface,
                         std::min(version, 1u)));
  } else if (strcmp(interface, tizen_move_resize_interface.name) == 0) {
    self->tizen_move_resize_ = static_cast<tizen_move_resize*>(
        wl_registry_bind(registry, name, &tizen_move_resize_interface,
                         std::min(version, 1u)));
  } else if (strcmp(interface, wl_text_input_manager_interface.name) == 0) {
    self->text_input_manager_ = static_cast<wl_text_input_manager*>(
        wl_registry_bind(registry, name, &wl_text_input_manager_interface,
                         std::min(version, 1u)));
  }
}

void TizenWindowEcoreWl2::HandleRegistryGlobalRemove(void* data,
                                                      wl_registry* registry,
                                                      uint32_t name) {}

void TizenWindowEcoreWl2::HandleXdgWmBasePing(void* data,
                                              xdg_wm_base* wm_base,
                                              uint32_t serial) {
  xdg_wm_base_pong(wm_base, serial);
}

void TizenWindowEcoreWl2::HandleXdgSurfaceConfigure(void* data,
                                                    xdg_surface* surface,
                                                    uint32_t serial) {
  TEMP_DIAG_ECORE_WL2("HandleXdgSurfaceConfigure serial=" << serial);
  auto* self = static_cast<TizenWindowEcoreWl2*>(data);
  if (!self) {
    return;
  }

  self->pending_geometry_serial_ = serial;
  xdg_surface_ack_configure(surface, serial);

  if (self->wl2_surface_) {
    wl_surface_commit(self->wl2_surface_);
    wl_display_flush(self->wl2_display_);
  }
}

void TizenWindowEcoreWl2::HandleXdgToplevelConfigure(void* data,
                                                     xdg_toplevel* toplevel,
                                                     int32_t width,
                                                     int32_t height,
                                                     wl_array* states) {
  auto* self = static_cast<TizenWindowEcoreWl2*>(data);
  if (!self) {
    return;
  }

  if (width > 0) {
    self->geometry_.width = width;
  }
  if (height > 0) {
    self->geometry_.height = height;
  }

  if (self->wl_egl_window_) {
    wl_egl_window_resize(self->wl_egl_window_, self->geometry_.width,
                         self->geometry_.height, 0, 0);
  }

  if (self->view_delegate_) {
    self->view_delegate_->OnResize(self->geometry_.left, self->geometry_.top,
                                   self->geometry_.width,
                                   self->geometry_.height);
  }
}

void TizenWindowEcoreWl2::HandleXdgToplevelClose(void* data,
                                                 xdg_toplevel* toplevel) {
  FT_LOG(Info) << "xdg_toplevel close requested.";
}

void TizenWindowEcoreWl2::HandleSeatCapabilities(void* data,
                                                 wl_seat* seat,
                                                 uint32_t capabilities) {
  auto* self = static_cast<TizenWindowEcoreWl2*>(data);
  if (!self) {
    return;
  }

  if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
    if (!self->pointer_) {
      self->pointer_ = wl_seat_get_pointer(seat);
      static const wl_pointer_listener kPointerListener = {
          HandlePointerEnter,
          HandlePointerLeave,
          HandlePointerMotion,
          HandlePointerButton,
          HandlePointerAxis,
      };
      wl_pointer_add_listener(self->pointer_, &kPointerListener, self);
    }
  } else if (self->pointer_) {
    wl_pointer_destroy(self->pointer_);
    self->pointer_ = nullptr;
  }

  if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
    if (!self->keyboard_) {
      self->keyboard_ = wl_seat_get_keyboard(seat);
      static const wl_keyboard_listener kKeyboardListener = {
          HandleKeyboardKeymap,
          HandleKeyboardEnter,
          HandleKeyboardLeave,
          HandleKeyboardKey,
          HandleKeyboardModifiers,
          HandleKeyboardRepeatInfo,
      };
      wl_keyboard_add_listener(self->keyboard_, &kKeyboardListener, self);
    }
  } else if (self->keyboard_) {
    wl_keyboard_destroy(self->keyboard_);
    self->keyboard_ = nullptr;
  }

  if (capabilities & WL_SEAT_CAPABILITY_TOUCH) {
    if (!self->touch_) {
      self->touch_ = wl_seat_get_touch(seat);
      static const wl_touch_listener kTouchListener = {
          HandleTouchDown,
          HandleTouchUp,
          HandleTouchMotion,
          HandleTouchFrame,
          HandleTouchCancel,
      };
      wl_touch_add_listener(self->touch_, &kTouchListener, self);
    }
  } else if (self->touch_) {
    wl_touch_destroy(self->touch_);
    self->touch_ = nullptr;
  }
}

void TizenWindowEcoreWl2::HandleSeatName(void* data,
                                         wl_seat* seat,
                                         const char* name) {
  auto* self = static_cast<TizenWindowEcoreWl2*>(data);
  if (!self) {
    return;
  }
  self->seat_name_ = name ? name : "";
}

void TizenWindowEcoreWl2::HandlePointerEnter(void* data,
                                             wl_pointer* pointer,
                                             uint32_t serial,
                                             wl_surface* surface,
                                             wl_fixed_t sx,
                                             wl_fixed_t sy) {
  auto* self = static_cast<TizenWindowEcoreWl2*>(data);
  if (!self || surface != self->wl2_surface_) {
    return;
  }

  self->last_input_serial_ = serial;
  self->pointer_x_ = wl_fixed_to_double(sx);
  self->pointer_y_ = wl_fixed_to_double(sy);

  if (self->default_cursor_ && self->cursor_surface_) {
    wl_cursor_image* image = self->default_cursor_->images[0];
    if (image) {
      wl_buffer* buffer = wl_cursor_image_get_buffer(image);
      wl_pointer_set_cursor(pointer, serial, self->cursor_surface_,
                            image->hotspot_x, image->hotspot_y);
      wl_surface_attach(self->cursor_surface_, buffer, 0, 0);
      wl_surface_damage(self->cursor_surface_, 0, 0, image->width,
                        image->height);
      wl_surface_commit(self->cursor_surface_);
      wl_display_flush(self->wl2_display_);
    }
  }

  // Do not emit hover move on enter; this frequently triggers an immediate
  // frame workload spike on some targets.
}

void TizenWindowEcoreWl2::HandlePointerLeave(void* data,
                                             wl_pointer* pointer,
                                             uint32_t serial,
                                             wl_surface* surface) {
  auto* self = static_cast<TizenWindowEcoreWl2*>(data);
  if (!self || surface != self->wl2_surface_) {
    return;
  }
  self->last_input_serial_ = serial;
}

void TizenWindowEcoreWl2::HandlePointerMotion(void* data,
                                              wl_pointer* pointer,
                                              uint32_t time,
                                              wl_fixed_t sx,
                                              wl_fixed_t sy) {
  auto* self = static_cast<TizenWindowEcoreWl2*>(data);
  if (!self) {
    return;
  }

  self->pointer_x_ = wl_fixed_to_double(sx);
  self->pointer_y_ = wl_fixed_to_double(sy);

  // Aggressive perf mode: suppress hover move dispatch entirely.
  // Pointer down/up/scroll events are still delivered.
  (void)time;
  return;
}

void TizenWindowEcoreWl2::HandlePointerButton(void* data,
                                              wl_pointer* pointer,
                                              uint32_t serial,
                                              uint32_t time,
                                              uint32_t button,
                                              uint32_t state) {
  auto* self = static_cast<TizenWindowEcoreWl2*>(data);
  if (!self || !self->view_delegate_) {
    return;
  }

#ifdef TV_PROFILE
  if ((!self->pointing_device_support_ || !self->floating_menu_support_) &&
      !self->show_unsupported_toast_) {
    self->show_unsupported_toast_ = true;
    return;
  }
#endif

  self->last_input_serial_ = serial;

  FlutterPointerMouseButtons flutter_button = ToFlutterPointerButton(button);
  if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
    self->pointer_button_pressed_ = true;
    self->view_delegate_->OnPointerDown(
        self->pointer_x_, self->pointer_y_, flutter_button,
        static_cast<size_t>(time), kFlutterPointerDeviceKindMouse, 0);
  } else {
    self->pointer_button_pressed_ = false;
    self->view_delegate_->OnPointerUp(self->pointer_x_, self->pointer_y_,
                                      flutter_button,
                                      static_cast<size_t>(time),
                                      kFlutterPointerDeviceKindMouse, 0);
  }
}

void TizenWindowEcoreWl2::HandlePointerAxis(void* data,
                                            wl_pointer* pointer,
                                            uint32_t time,
                                            uint32_t axis,
                                            wl_fixed_t value) {
  auto* self = static_cast<TizenWindowEcoreWl2*>(data);
  if (!self || !self->view_delegate_) {
    return;
  }

  double delta_x = 0.0;
  double delta_y = 0.0;
  if (axis == kScrollDirectionVertical) {
    delta_y = wl_fixed_to_double(value);
  } else if (axis == kScrollDirectionHorizontal) {
    delta_x = wl_fixed_to_double(value);
  }

  self->view_delegate_->OnScroll(self->pointer_x_, self->pointer_y_, delta_x,
                                 delta_y, static_cast<size_t>(time),
                                 kFlutterPointerDeviceKindMouse, 0);
}

void TizenWindowEcoreWl2::HandleKeyboardKeymap(void* data,
                                               wl_keyboard* keyboard,
                                               uint32_t format,
                                               int32_t fd,
                                               uint32_t size) {
  auto* self = static_cast<TizenWindowEcoreWl2*>(data);
  if (!self) {
    close(fd);
    return;
  }

  if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
    close(fd);
    return;
  }

  char* keymap_string = static_cast<char*>(
      mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
  if (keymap_string == MAP_FAILED) {
    close(fd);
    return;
  }

  if (!self->keyboard_state_.context) {
    self->keyboard_state_.context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  }

  if (!self->keyboard_state_.context) {
    munmap(keymap_string, size);
    close(fd);
    return;
  }

  xkb_keymap* keymap = xkb_keymap_new_from_string(
      self->keyboard_state_.context, keymap_string,
      XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);

  munmap(keymap_string, size);
  close(fd);

  if (!keymap) {
    FT_LOG(Error) << "Failed to create xkb keymap.";
    return;
  }

  xkb_state* state = xkb_state_new(keymap);
  if (!state) {
    xkb_keymap_unref(keymap);
    FT_LOG(Error) << "Failed to create xkb state.";
    return;
  }

  if (self->keyboard_state_.state) {
    xkb_state_unref(self->keyboard_state_.state);
  }
  if (self->keyboard_state_.keymap) {
    xkb_keymap_unref(self->keyboard_state_.keymap);
  }

  self->keyboard_state_.keymap = keymap;
  self->keyboard_state_.state = state;

  self->keyboard_state_.shift_mod =
      xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_SHIFT);
  self->keyboard_state_.ctrl_mod =
      xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_CTRL);
  self->keyboard_state_.alt_mod =
      xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_ALT);
  self->keyboard_state_.logo_mod =
      xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_LOGO);

  self->keyboard_state_.num_led =
      xkb_keymap_led_get_index(keymap, XKB_LED_NAME_NUM);
  self->keyboard_state_.caps_led =
      xkb_keymap_led_get_index(keymap, XKB_LED_NAME_CAPS);
  self->keyboard_state_.scroll_led =
      xkb_keymap_led_get_index(keymap, XKB_LED_NAME_SCROLL);
}

void TizenWindowEcoreWl2::HandleKeyboardEnter(void* data,
                                              wl_keyboard* keyboard,
                                              uint32_t serial,
                                              wl_surface* surface,
                                              wl_array* keys) {
  auto* self = static_cast<TizenWindowEcoreWl2*>(data);
  if (!self || surface != self->wl2_surface_) {
    return;
  }
  self->last_input_serial_ = serial;
}

void TizenWindowEcoreWl2::HandleKeyboardLeave(void* data,
                                              wl_keyboard* keyboard,
                                              uint32_t serial,
                                              wl_surface* surface) {
  auto* self = static_cast<TizenWindowEcoreWl2*>(data);
  if (!self || surface != self->wl2_surface_) {
    return;
  }
  self->last_input_serial_ = serial;
}

void TizenWindowEcoreWl2::HandleKeyboardKey(void* data,
                                            wl_keyboard* keyboard,
                                            uint32_t serial,
                                            uint32_t time,
                                            uint32_t key,
                                            uint32_t state) {
  auto* self = static_cast<TizenWindowEcoreWl2*>(data);
  if (!self || !self->view_delegate_) {
    return;
  }

  if (!self->keyboard_state_.state) {
    return;
  }

  self->last_input_serial_ = serial;

  xkb_keycode_t keycode = key + 8;
  xkb_keysym_t keysym =
      xkb_state_key_get_one_sym(self->keyboard_state_.state, keycode);

  std::array<char, 64> key_name{};
  const char* key_symbol = "";
  if (keysym != XKB_KEY_NoSymbol &&
      xkb_keysym_get_name(keysym, key_name.data(), key_name.size()) > 0) {
    key_symbol = key_name.data();
  }

  std::array<char, 64> text{};
  const char* text_ptr = nullptr;
  int text_size =
      xkb_state_key_get_utf8(self->keyboard_state_.state, keycode, text.data(),
                             text.size());
  if (text_size > 0 && text[0] != '\0') {
    text_ptr = text.data();
  }

  bool is_down = state == WL_KEYBOARD_KEY_STATE_PRESSED;
  bool handled = false;

  if (self->input_method_context_ &&
      self->input_method_context_->IsInputPanelShown()) {
    handled = self->input_method_context_->HandleKeyEvent(
        self->seat_name_.empty() ? nullptr : self->seat_name_.c_str(),
        kEcoreDeviceClassKeyboard, kEcoreDeviceSubclassNone, key_symbol,
        text_ptr, self->keyboard_state_.ecore_style_modifiers, keycode, time,
        is_down);
  }

  if (!handled) {
    self->view_delegate_->OnKey(
        key_symbol, text_ptr, text_ptr, self->keyboard_state_.ecore_style_modifiers,
        keycode,
        is_down && !self->seat_name_.empty() ? self->seat_name_.c_str()
                                             : nullptr,
        is_down);
  }
}

void TizenWindowEcoreWl2::HandleKeyboardModifiers(void* data,
                                                  wl_keyboard* keyboard,
                                                  uint32_t serial,
                                                  uint32_t depressed,
                                                  uint32_t latched,
                                                  uint32_t locked,
                                                  uint32_t group) {
  auto* self = static_cast<TizenWindowEcoreWl2*>(data);
  if (!self) {
    return;
  }

  self->last_input_serial_ = serial;
  self->UpdateKeyboardModifiers(depressed, latched, locked, group);
}

void TizenWindowEcoreWl2::HandleKeyboardRepeatInfo(void* data,
                                                   wl_keyboard* keyboard,
                                                   int32_t rate,
                                                   int32_t delay) {}

void TizenWindowEcoreWl2::HandleTouchDown(void* data,
                                          wl_touch* touch,
                                          uint32_t serial,
                                          uint32_t time,
                                          wl_surface* surface,
                                          int32_t id,
                                          wl_fixed_t x,
                                          wl_fixed_t y) {
  auto* self = static_cast<TizenWindowEcoreWl2*>(data);
  if (!self || surface != self->wl2_surface_ || !self->view_delegate_) {
    return;
  }

  self->last_input_serial_ = serial;
  double touch_x = wl_fixed_to_double(x);
  double touch_y = wl_fixed_to_double(y);
  self->touch_points_[id] = {touch_x, touch_y};

  self->view_delegate_->OnPointerDown(
      touch_x, touch_y, kFlutterPointerButtonMousePrimary,
      static_cast<size_t>(time), kFlutterPointerDeviceKindTouch, id);
}

void TizenWindowEcoreWl2::HandleTouchUp(void* data,
                                        wl_touch* touch,
                                        uint32_t serial,
                                        uint32_t time,
                                        int32_t id) {
  auto* self = static_cast<TizenWindowEcoreWl2*>(data);
  if (!self || !self->view_delegate_) {
    return;
  }

  self->last_input_serial_ = serial;

  double touch_x = 0.0;
  double touch_y = 0.0;
  auto iter = self->touch_points_.find(id);
  if (iter != self->touch_points_.end()) {
    touch_x = iter->second.first;
    touch_y = iter->second.second;
    self->touch_points_.erase(iter);
  }

  self->view_delegate_->OnPointerUp(
      touch_x, touch_y, kFlutterPointerButtonMousePrimary,
      static_cast<size_t>(time), kFlutterPointerDeviceKindTouch, id);
}

void TizenWindowEcoreWl2::HandleTouchMotion(void* data,
                                            wl_touch* touch,
                                            uint32_t time,
                                            int32_t id,
                                            wl_fixed_t x,
                                            wl_fixed_t y) {
  auto* self = static_cast<TizenWindowEcoreWl2*>(data);
  if (!self || !self->view_delegate_) {
    return;
  }

  double touch_x = wl_fixed_to_double(x);
  double touch_y = wl_fixed_to_double(y);
  self->touch_points_[id] = {touch_x, touch_y};

  self->view_delegate_->OnPointerMove(touch_x, touch_y, static_cast<size_t>(time),
                                      kFlutterPointerDeviceKindTouch, id);
}

void TizenWindowEcoreWl2::HandleTouchFrame(void* data, wl_touch* touch) {}

void TizenWindowEcoreWl2::HandleTouchCancel(void* data, wl_touch* touch) {
  auto* self = static_cast<TizenWindowEcoreWl2*>(data);
  if (!self) {
    return;
  }
  self->touch_points_.clear();
}

void TizenWindowEcoreWl2::HandleOutputGeometry(void* data,
                                               wl_output* output,
                                               int32_t x,
                                               int32_t y,
                                               int32_t physical_width,
                                               int32_t physical_height,
                                               int32_t subpixel,
                                               const char* make,
                                               const char* model,
                                               int32_t transform) {
  auto* self = static_cast<TizenWindowEcoreWl2*>(data);
  if (!self) {
    return;
  }

  self->output_transform_ = transform;
  self->rotation_degree_ = ToRotationDegree(transform);
  self->output_physical_width_mm_ = physical_width;
  self->output_physical_height_mm_ = physical_height;
  self->UpdateOutputDpi();
}

void TizenWindowEcoreWl2::HandleOutputMode(void* data,
                                           wl_output* output,
                                           uint32_t flags,
                                           int32_t width,
                                           int32_t height,
                                           int32_t refresh) {
  auto* self = static_cast<TizenWindowEcoreWl2*>(data);
  if (!self) {
    return;
  }

  if (flags & WL_OUTPUT_MODE_CURRENT) {
    self->screen_geometry_.width = width;
    self->screen_geometry_.height = height;
    self->UpdateOutputDpi();
  }
}

void TizenWindowEcoreWl2::HandleOutputDone(void* data, wl_output* output) {
  auto* self = static_cast<TizenWindowEcoreWl2*>(data);
  if (!self || !self->view_delegate_) {
    return;
  }
  self->view_delegate_->OnRotate(self->rotation_degree_);
}

void TizenWindowEcoreWl2::HandleOutputScale(void* data,
                                            wl_output* output,
                                            int32_t factor) {}

void TizenWindowEcoreWl2::HandleResourceId(void* data,
                                           tizen_resource* tizen_resource,
                                           uint32_t id) {
  auto* self = static_cast<TizenWindowEcoreWl2*>(data);
  if (!self) {
    return;
  }
  self->resource_id_ = id;
}

}  // namespace flutter
