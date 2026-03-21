// Copyright 2022 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tizen_window_ecore_wl2.h"

#ifdef TV_PROFILE
#include <app.h>
#include <app_preference.h>
#include <dlfcn.h>
#include <time.h>
#include <vconf.h>

#include <cstring>
#endif

#include <memory>
#include <sstream>
#include <tuple>

#include "flutter/shell/platform/embedder/embedder.h"
#include "flutter/shell/platform/tizen/logger.h"
#include "flutter/shell/platform/tizen/tizen_view_event_handler_delegate.h"

namespace flutter {

namespace {

constexpr int kScrollDirectionVertical = 0;
constexpr int kScrollDirectionHorizontal = 1;

#ifdef TV_PROFILE
constexpr char kSysMouseCursorPointerSizeVConfKey[] =
    "db/menu/system/mouse-pointer-size";
constexpr char kEcoreWL2InputCursorThemeName[] = "vd-cursors";
#endif

using TcWindow = tizen_core_wayland::Window;
using RotationAngle = tizen_core_wayland::Window::RotationAngle;

FlutterPointerMouseButtons ToFlutterPointerButton(int32_t button) {
  if (button == 2) {
    return kFlutterPointerButtonMouseMiddle;
  } else if (button == 3) {
    return kFlutterPointerButtonMouseSecondary;
  } else {
    return kFlutterPointerButtonMousePrimary;
  }
}

FlutterPointerDeviceKind ToFlutterDeviceKind(
    const tizen_core_wayland::InputDevice* dev) {
  if (!dev) {
    return kFlutterPointerDeviceKindTouch;
  }

  switch (dev->GetClass()) {
    case tizen_core_wayland::InputDevice::Type::Mouse:
      return kFlutterPointerDeviceKindMouse;
    case tizen_core_wayland::InputDevice::Type::Pen:
      return kFlutterPointerDeviceKindStylus;
    default:
      return kFlutterPointerDeviceKindTouch;
  }
}

uint32_t ToImfDeviceClass(const tizen_core_wayland::InputDevice* dev) {
  return dev ? static_cast<uint32_t>(dev->GetClass()) : 0U;
}

uint32_t ToImfDeviceSubclass(const tizen_core_wayland::InputDevice* dev) {
  if (!dev) {
    return 0U;
  }

  switch (dev->GetSubClass()) {
    case tizen_core_wayland::InputDevice::Subtype::Remocon:
      return 11U;
    case tizen_core_wayland::InputDevice::Subtype::VirtualKeyboard:
      return 12U;
    default:
      return 0U;
  }
}

RotationAngle ToRotationAngle(int degree) {
  switch (degree) {
    case 0:
      return RotationAngle::Landscape;
    case 90:
      return RotationAngle::Portrait;
    case 180:
      return RotationAngle::LandscapeInverse;
    case 270:
      return RotationAngle::PortraitInverse;
    default:
      return RotationAngle::Landscape;
  }
}

int FromRotationAngle(RotationAngle angle) {
  switch (angle) {
    case RotationAngle::Portrait:
      return 90;
    case RotationAngle::LandscapeInverse:
      return 180;
    case RotationAngle::PortraitInverse:
      return 270;
    case RotationAngle::Landscape:
    default:
      return 0;
  }
}

std::vector<RotationAngle> ToRotationAngles(const std::vector<int>& rotations) {
  std::vector<RotationAngle> result;
  result.reserve(rotations.size());
  for (int rotation : rotations) {
    result.push_back(ToRotationAngle(rotation));
  }
  return result;
}

#ifdef TV_PROFILE
time_t GetBootTimeEpoch() {
  struct timespec now, boot_time;
  if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
    FT_LOG(Error) << "Fail to get clock_gettime(CLOCK_REALTIME).";
    return -1;
  }
  if (clock_gettime(CLOCK_BOOTTIME, &boot_time) != 0) {
    FT_LOG(Error) << "Fail to get clock_gettime(CLOCK_BOOTTIME).";
    return -1;
  }
  time_t boot_time_epoch = now.tv_sec - boot_time.tv_sec;
  return (boot_time_epoch / 10) * 10;
}

bool PreferenceItemCallback(const char* key, void* user_data) {
  char* app_id = static_cast<char*>(user_data);
  if (!app_id || !key) {
    return true;
  }

  std::string preference_key = std::string("flutter-tizen/preference/"
                                           "pointing-device-support-toast") +
                               "/" + app_id;
  if (!strncmp(key, preference_key.c_str(), preference_key.length())) {
    preference_remove(key);
  }
  return true;
}

std::string GetPreferenceKey(bool clear_exist_key) {
  time_t boot_time = GetBootTimeEpoch();
  if (boot_time == -1) {
    return "";
  }

  char* id = nullptr;
  int ret = app_get_id(&id);
  if (ret != APP_CONTROL_ERROR_NONE || !id) {
    FT_LOG(Error) << "Fail to get app id.";
    return std::string();
  }

  std::string app_id = id;
  free(id);

  std::ostringstream boot_time_buffer;
  boot_time_buffer << boot_time;

  std::string preference_key =
      "flutter-tizen/preference/pointing-device-support-toast/" + app_id +
      "/" + boot_time_buffer.str();

  if (clear_exist_key) {
    preference_foreach_item(PreferenceItemCallback, (void*)app_id.c_str());
  }
  return preference_key;
}

bool GetPointingDeviceToastPreference() {
  bool show_unsupported_toast = false;
  std::string preference_key = GetPreferenceKey(false);
  if (preference_key.empty()) {
    return false;
  }

  int ret =
      preference_get_boolean(preference_key.c_str(), &show_unsupported_toast);
  if (ret != PREFERENCE_ERROR_NONE) {
    return false;
  }
  return show_unsupported_toast;
}

void SetPointingDevicePreference() {
  std::string preference_key = GetPreferenceKey(true);
  if (preference_key.empty()) {
    return;
  }

  int ret = preference_set_boolean(preference_key.c_str(), true);
  if (ret != PREFERENCE_ERROR_NONE) {
    FT_LOG(Error) << "Fail to set toasted preference.";
  }
}
#endif

}  // namespace

TizenWindowEcoreWl2::TizenWindowEcoreWl2(TizenGeometry geometry,
                                         bool transparent,
                                         bool focusable,
                                         bool top_level,
                                         bool pointing_device_support,
                                         bool floating_menu_support,
                                         void* window_handle,
                                         bool is_vulkan)
    : TizenWindow(geometry, transparent, focusable, top_level)
#ifdef TV_PROFILE
      ,
      pointing_device_support_(pointing_device_support),
      floating_menu_support_(floating_menu_support)
#endif
      ,
      is_vulkan_(is_vulkan) {
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
  UnregisterEventHandlers();
  DestroyWindow();
}

bool TizenWindowEcoreWl2::CreateWindow(void* window_handle) {
  if (window_handle) {
    window_ = static_cast<tizen_core_wayland::Window*>(window_handle);
    if (!window_) {
      return false;
    }
    display_ = window_->GetDisplay();
    if (display_ && !display_->IsConnected()) {
      display_->Connect();
      display_->Sync();
    }
    auto geometry = window_->GetGeometry();
    if (initial_geometry_.width == 0) {
      initial_geometry_.width = geometry.w;
    }
    if (initial_geometry_.height == 0) {
      initial_geometry_.height = geometry.h;
    }
  } else {
    std::ostringstream stream;
    stream << "flutter-embedder-" << this;
    display_name_ = stream.str();
    display_ =
        &tizen_core_wayland::DisplayManager::GetInst().Create(display_name_);
    owns_display_ = true;
    display_->Connect();
    display_->Sync();

    auto [screen_width, screen_height] = display_->GetScreenSize();
    if (screen_width == 0 || screen_height == 0) {
      FT_LOG(Error) << "Invalid screen size: " << screen_width << " x "
                    << screen_height;
      return false;
    }

    if (initial_geometry_.width == 0) {
      initial_geometry_.width = screen_width;
    }
    if (initial_geometry_.height == 0) {
      initial_geometry_.height = screen_height;
    }

    window_ = display_->CreateWindow(nullptr, initial_geometry_.left,
                                     initial_geometry_.top,
                                     initial_geometry_.width,
                                     initial_geometry_.height);
    owns_window_ = window_ != nullptr;
  }

  if (!window_ || !display_) {
    return false;
  }

  wl2_display_ = display_->GetWlDisplay();
  wl2_surface_ = window_->GetWlSurface();
  input_ = display_->FindDefaultInput();

  if (is_vulkan_) {
    return wl2_surface_ && wl2_display_;
  }

  egl_window_ = wl_egl_window_tizen_create(
      wl2_surface_, initial_geometry_.width, initial_geometry_.height);
  return egl_window_ && wl2_display_;
}

void TizenWindowEcoreWl2::SetWindowOptions() {
  if (!window_) {
    return;
  }

  window_->SetType(top_level_ ? TcWindow::Type::Notification
                              : TcWindow::Type::TopLevel);
  if (top_level_) {
    SetTizenPolicyNotificationLevel(TIZEN_POLICY_LEVEL_TOP);
  }

  window_->SetPosition(initial_geometry_.left, initial_geometry_.top);
  window_->SetGeometry({initial_geometry_.left, initial_geometry_.top,
                        initial_geometry_.width, initial_geometry_.height});
  window_->SetAlpha(transparent_);
  if (!focusable_) {
    window_->SetFocusSkip(true);
  }
  window_->SetIndicatorEnable(true);

#ifdef TV_PROFILE
  std::vector<int> rotations = {0};
#else
  std::vector<int> rotations = {0, 90, 180, 270};
#endif
  window_->SetRotationAvailableAngles(ToRotationAngles(rotations));
  EnableCursor();
  window_->Commit(true);
}

void TizenWindowEcoreWl2::EnableCursor() {
#ifdef TV_PROFILE
  void* handle = dlopen("libvd-win-util.so", RTLD_LAZY);
  if (!handle) {
    FT_LOG(Error) << "Could not open a shared library libvd-win-util.so.";
    return;
  }

  int (*CursorModule_Initialize)(wl_display* display, wl_registry* registry,
                                 wl_seat* seat, unsigned int id);
  int (*Cursor_Set_Config)(wl_surface* surface, uint32_t config_type,
                           void* data);
  void (*CursorModule_Finalize)(void);
  *(void**)(&CursorModule_Initialize) =
      dlsym(handle, "CursorModule_Initialize");
  *(void**)(&Cursor_Set_Config) = dlsym(handle, "Cursor_Set_Config");
  *(void**)(&CursorModule_Finalize) = dlsym(handle, "CursorModule_Finalize");

  if (!CursorModule_Initialize || !Cursor_Set_Config ||
      !CursorModule_Finalize || !display_ || !input_) {
    FT_LOG(Error) << "Could not load cursor module symbols.";
    dlclose(handle);
    return;
  }

  wl_registry* registry = display_->GetRegistry();
  wl_seat* seat = input_->GetWlSeat();
  if (!registry || !seat) {
    FT_LOG(Error) << "Could not retreive wl_registry or wl_seat.";
    dlclose(handle);
    return;
  }

  for (const auto& [id, global] : display_->GetGlobals()) {
    if (global.GetInterface() == "tizen_cursor") {
      if (!CursorModule_Initialize(wl2_display_, registry, seat, id)) {
        FT_LOG(Error) << "Failed to initialize the cursor module.";
      }
      break;
    }
  }

  display_->Sync();
  if (!Cursor_Set_Config(window_->GetWlSurface(), 1, nullptr)) {
    FT_LOG(Error) << "Failed to set a cursor config value.";
  }

  CursorModule_Finalize();
  dlclose(handle);
#endif
}

#ifdef TV_PROFILE
void TizenWindowEcoreWl2::SetPointingDeviceSupport() {
  FT_LOG(Info) << "Pointing device support toggle is not available without "
                  "the legacy window backend.";
}

void TizenWindowEcoreWl2::SetFloatingMenuSupport() {
  FT_LOG(Info) << "Floating menu support toggle is not available without "
                  "the legacy window backend.";
}

void TizenWindowEcoreWl2::ShowUnsupportedToast() {
  FT_LOG(Info) << "Unsupported toast launch is not available without "
                  "the legacy window backend.";
}
#endif

void TizenWindowEcoreWl2::RegisterEventHandlers() {
  auto& broker = display_->GetEventBroker();

  event_handlers_.push_back(broker.AddListener(
      tizen_core_wayland::EventType::WindowRotate,
      [this](const tizen_core_wayland::EventBase* event) {
        auto* rotation_event =
            static_cast<const tizen_core_wayland::WindowRotateEvent*>(event);
        if (!view_delegate_ || rotation_event->GetWindowId() != GetWindowId()) {
          return;
        }

        view_delegate_->OnRotate(rotation_event->GetAngle());
        if (egl_window_ && rotation_event->IsResized()) {
          wl_egl_window_tizen_resize(egl_window_, rotation_event->GetWidth(),
                                     rotation_event->GetHeight(), 0, 0);
        }
      }));

  if (!is_vulkan_) {
    event_handlers_.push_back(broker.AddListener(
        tizen_core_wayland::EventType::WindowConfigure,
        [this](const tizen_core_wayland::EventBase* event) {
          auto* configure_event =
              static_cast<const tizen_core_wayland::WindowConfigureEvent*>(
                  event);
          if (!view_delegate_ ||
              configure_event->GetWindowId() != GetWindowId()) {
            return;
          }

          auto [x, y, w, h] = configure_event->GetPosition();
          if (egl_window_) {
            wl_egl_window_tizen_resize(egl_window_, w, h, 0, 0);
          }
          view_delegate_->OnResize(x, y, w, h);
        }));
  }

  event_handlers_.push_back(broker.AddListener(
      tizen_core_wayland::EventType::MouseButtonDown,
      [this](const tizen_core_wayland::EventBase* event) {
        auto* button_event =
            static_cast<const tizen_core_wayland::MouseButtonDownEvent*>(event);
        if (!view_delegate_ || button_event->GetWindowId() != GetWindowId()) {
          return;
        }

#ifdef TV_PROFILE
        if ((!pointing_device_support_ || !floating_menu_support_) &&
            !show_unsupported_toast_) {
          bool shown = GetPointingDeviceToastPreference();
          if (!shown) {
            SetPointingDevicePreference();
          }
          show_unsupported_toast_ = true;
        }
#endif
        view_delegate_->OnPointerDown(
            button_event->GetX(), button_event->GetY(),
            ToFlutterPointerButton(button_event->GetButtons()),
            button_event->GetTimestamp(),
            ToFlutterDeviceKind(button_event->GetInputDevice()),
            button_event->GetDeviceId());
      }));

  event_handlers_.push_back(broker.AddListener(
      tizen_core_wayland::EventType::MouseButtonUp,
      [this](const tizen_core_wayland::EventBase* event) {
        auto* button_event =
            static_cast<const tizen_core_wayland::MouseButtonUpEvent*>(event);
        if (!view_delegate_ || button_event->GetWindowId() != GetWindowId()) {
          return;
        }

        view_delegate_->OnPointerUp(
            button_event->GetX(), button_event->GetY(),
            ToFlutterPointerButton(button_event->GetButtons()),
            button_event->GetTimestamp(),
            ToFlutterDeviceKind(button_event->GetInputDevice()),
            button_event->GetDeviceId());
      }));

  event_handlers_.push_back(broker.AddListener(
      tizen_core_wayland::EventType::MouseMove,
      [this](const tizen_core_wayland::EventBase* event) {
        auto* move_event =
            static_cast<const tizen_core_wayland::MouseMoveEvent*>(event);
        if (!view_delegate_ || move_event->GetWindowId() != GetWindowId()) {
          return;
        }

        view_delegate_->OnPointerMove(
            move_event->GetX(), move_event->GetY(), move_event->GetTimestamp(),
            ToFlutterDeviceKind(move_event->GetInputDevice()),
            move_event->GetDeviceId());
      }));

  event_handlers_.push_back(broker.AddListener(
      tizen_core_wayland::EventType::MouseWheel,
      [this](const tizen_core_wayland::EventBase* event) {
        auto* wheel_event =
            static_cast<const tizen_core_wayland::MouseWheelEvent*>(event);
        if (!view_delegate_ || wheel_event->GetWindowId() != GetWindowId()) {
          return;
        }

        double delta_x = 0.0;
        double delta_y = 0.0;
        if (wheel_event->GetDirection() == kScrollDirectionVertical) {
          delta_y += wheel_event->GetZ();
        } else if (wheel_event->GetDirection() ==
                   kScrollDirectionHorizontal) {
          delta_x += wheel_event->GetZ();
        }

        view_delegate_->OnScroll(
            wheel_event->GetX(), wheel_event->GetY(), delta_x, delta_y,
            wheel_event->GetTimestamp(),
            ToFlutterDeviceKind(wheel_event->GetInputDevice()), 0);
      }));

  event_handlers_.push_back(broker.AddListener(
      tizen_core_wayland::EventType::KeyDown,
      [this](const tizen_core_wayland::EventBase* event) {
        auto* key_event =
            static_cast<const tizen_core_wayland::KeyDownEvent*>(event);
        if (!view_delegate_ || key_event->GetWindowId() != GetWindowId()) {
          return;
        }

        auto* dev = key_event->GetInputDevice();
        std::string device_name = dev ? dev->GetName() : "";
        bool handled = false;
        if (input_method_context_ && input_method_context_->IsInputPanelShown()) {
          handled = input_method_context_->HandleKeyEvent(
              device_name.empty() ? nullptr : device_name.c_str(),
              ToImfDeviceClass(dev), ToImfDeviceSubclass(dev),
              key_event->GetKey().c_str(), key_event->GetKeyName().c_str(),
              key_event->GetString().c_str(), key_event->GetCompose().c_str(),
              key_event->GetModifiers(), key_event->GetKeycode(),
              key_event->GetTimestamp(), true);
        }
        if (!handled) {
          view_delegate_->OnKey(
              key_event->GetKey().c_str(), key_event->GetString().c_str(),
              key_event->GetCompose().c_str(), key_event->GetModifiers(),
              key_event->GetKeycode(),
              device_name.empty() ? nullptr : device_name.c_str(), true);
        }
      }));

  event_handlers_.push_back(broker.AddListener(
      tizen_core_wayland::EventType::KeyUp,
      [this](const tizen_core_wayland::EventBase* event) {
        auto* key_event =
            static_cast<const tizen_core_wayland::KeyUpEvent*>(event);
        if (!view_delegate_ || key_event->GetWindowId() != GetWindowId()) {
          return;
        }

        auto* dev = key_event->GetInputDevice();
        std::string device_name = dev ? dev->GetName() : "";
        bool handled = false;
        if (input_method_context_ && input_method_context_->IsInputPanelShown()) {
          handled = input_method_context_->HandleKeyEvent(
              device_name.empty() ? nullptr : device_name.c_str(),
              ToImfDeviceClass(dev), ToImfDeviceSubclass(dev),
              key_event->GetKey().c_str(), key_event->GetKeyName().c_str(),
              key_event->GetString().c_str(), key_event->GetCompose().c_str(),
              key_event->GetModifiers(), key_event->GetKeycode(),
              key_event->GetTimestamp(), false);
        }
        if (!handled) {
          view_delegate_->OnKey(key_event->GetKey().c_str(),
                                key_event->GetString().c_str(),
                                key_event->GetCompose().c_str(),
                                key_event->GetModifiers(),
                                key_event->GetKeycode(), nullptr, false);
        }
      }));
}

void TizenWindowEcoreWl2::UnregisterEventHandlers() {
  if (!display_) {
    event_handlers_.clear();
    return;
  }

  auto& broker = display_->GetEventBroker();
  for (auto handler : event_handlers_) {
    broker.RemoveListener(handler);
  }
  event_handlers_.clear();
}

void TizenWindowEcoreWl2::DestroyWindow() {
  if (egl_window_) {
    wl_egl_window_tizen_destroy(egl_window_);
    egl_window_ = nullptr;
  }

  if (owns_window_ && display_ && window_) {
    display_->DestroyWindow(window_);
  }
  window_ = nullptr;
  input_ = nullptr;
  wl2_surface_ = nullptr;
  wl2_display_ = nullptr;

  if (owns_display_) {
    if (display_) {
      display_->Disconnect();
    }
    tizen_core_wayland::DisplayManager::GetInst().Destroy(display_name_);
  }
  display_ = nullptr;
  owns_window_ = false;
  owns_display_ = false;
}

TizenGeometry TizenWindowEcoreWl2::GetGeometry() {
  if (!window_) {
    return {};
  }

  auto geometry = window_->GetGeometry();
  return {geometry.x, geometry.y, geometry.w, geometry.h};
}

bool TizenWindowEcoreWl2::SetGeometry(TizenGeometry geometry) {
  if (!window_) {
    return false;
  }

  TcWindow::Rect rect = {geometry.left, geometry.top, geometry.width,
                         geometry.height};
  window_->SetRotationGeometry(ToRotationAngle(GetRotation()), rect);
  window_->SetGeometry(rect);
  window_->Commit(true);
  return true;
}

TizenGeometry TizenWindowEcoreWl2::GetScreenGeometry() {
  if (!display_) {
    return {};
  }
  auto [width, height] = display_->GetScreenSize();
  return {0, 0, width, height};
}

int32_t TizenWindowEcoreWl2::GetRotation() {
  if (!window_) {
    return 0;
  }
  return FromRotationAngle(window_->GetRotation());
}

int32_t TizenWindowEcoreWl2::GetDpi() {
  if (!display_) {
    return 0;
  }

  if (window_) {
    if (auto* output = window_->FindOutput()) {
      return output->GetDpi();
    }
  }

  auto& outputs = display_->GetOutputs();
  if (!outputs.empty()) {
    return outputs.front()->GetDpi();
  }
  return 0;
}

uintptr_t TizenWindowEcoreWl2::GetWindowId() {
  return window_ ? window_->GetId() : 0U;
}

uint32_t TizenWindowEcoreWl2::GetResourceId() {
  if (resource_id_ == 0 && window_) {
    resource_id_ = window_->GetResourceId();
  }
  return resource_id_;
}

void TizenWindowEcoreWl2::SetPreferredOrientations(
    const std::vector<int>& rotations) {
  if (window_) {
    window_->SetRotationAvailableAngles(ToRotationAngles(rotations));
  }
}

void TizenWindowEcoreWl2::BindKeys(const std::vector<std::string>& keys) {
  if (!window_) {
    return;
  }

  std::vector<TcWindow::KeygrabInfo> infos;
  infos.reserve(keys.size());
  for (const auto& key : keys) {
    infos.push_back(
        {key, tizen_core_wayland::Window::KeygrabMode::Topmost});
  }
  window_->SetKeygrabList(infos);
}

void TizenWindowEcoreWl2::Show() {
  if (window_) {
    window_->Show();
    window_->Commit(true);
  }
}

void TizenWindowEcoreWl2::UpdateFlutterCursor(const std::string& kind) {
#ifdef TV_PROFILE
  if (!input_) {
    return;
  }

  int pointer_size = -1;
  if (vconf_get_int(kSysMouseCursorPointerSizeVConfKey, &pointer_size) < 0) {
    FT_LOG(Info) << "Failed to load cursor size.";
  }

  std::string cursor_name = "normal_default";
  if (kind == "basic") {
    if (pointer_size == 0) {
      cursor_name = "large_normal";
    } else if (pointer_size == 1) {
      cursor_name = "medium_normal";
    } else if (pointer_size == 2) {
      cursor_name = "small_normal";
    }
  } else if (kind == "click") {
    if (pointer_size == 0) {
      cursor_name = "large_normal_pnh";
    } else if (pointer_size == 1) {
      cursor_name = "medium_normal_pnh";
    } else if (pointer_size == 2) {
      cursor_name = "small_normal_pnh";
    } else {
      cursor_name = "normal_pnh";
    }
  } else if (kind == "text") {
    if (pointer_size == 0) {
      cursor_name = "large_normal_input_field";
    } else if (pointer_size == 1) {
      cursor_name = "medium_normal_input_field";
    } else if (pointer_size == 2) {
      cursor_name = "small_normal_input_field";
    } else {
      cursor_name = "normal_input_field";
    }
  } else if (kind == "none") {
    cursor_name = "normal_transparent";
  } else {
    FT_LOG(Info) << kind << " cursor is not supported.";
  }

  input_->SetCursorThemeName(kEcoreWL2InputCursorThemeName);
  input_->SetCursor(cursor_name);
#else
  FT_LOG(Info) << "UpdateFlutterCursor is not supported.";
#endif
}

void TizenWindowEcoreWl2::SetTizenPolicyNotificationLevel(int level) {
  if (!display_ || !window_) {
    return;
  }

  auto* policy = display_->GetTzPolicy();
  if (!policy) {
    FT_LOG(Error)
        << "Failed to initialize the tizen policy handle, the top_level "
           "attribute is ignored.";
    return;
  }

  tizen_policy_set_notification_level(policy, window_->GetWlSurface(), level);
}

void TizenWindowEcoreWl2::PrepareInputMethod() {
  input_method_context_ =
      std::make_unique<TizenInputMethodContext>(GetNativeHandle());

  input_method_context_->SetOnPreeditStart(
      [this]() { view_delegate_->OnComposeBegin(); });
  input_method_context_->SetOnPreeditChanged(
      [this](std::string str, int cursor_pos) {
        view_delegate_->OnComposeChange(str, cursor_pos);
      });
  input_method_context_->SetOnPreeditEnd(
      [this]() { view_delegate_->OnComposeEnd(); });
  input_method_context_->SetOnCommit(
      [this](std::string str) { view_delegate_->OnCommit(str); });
}

void* TizenWindowEcoreWl2::GetRenderTarget() {
  if (is_vulkan_) {
    return wl2_surface_;
  }
  return egl_window_;
}

void TizenWindowEcoreWl2::ActivateWindow() {
  if (window_) {
    window_->Activate();
  }
}

void TizenWindowEcoreWl2::RaiseWindow() {
  if (window_) {
    window_->Raise();
  }
}

void TizenWindowEcoreWl2::LowerWindow() {
  if (window_) {
    window_->Lower();
  }
}

}  // namespace flutter
