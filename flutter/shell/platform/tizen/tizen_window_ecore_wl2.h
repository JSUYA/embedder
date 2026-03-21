// Copyright 2022 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_TIZEN_WINDOW_ECORE_WL2_H_
#define EMBEDDER_TIZEN_WINDOW_ECORE_WL2_H_

#include <tizen-extension-client-protocol.h>
#include <wayland-egl-tizen.h>

#include <cstdint>
#include <string>
#include <vector>

#include <tizen_core_wayland.h>

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

  void* GetNativeHandle() override { return window_; }

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

 private:
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

  tizen_core_wayland::Display* display_ = nullptr;
  tizen_core_wayland::Window* window_ = nullptr;
  tizen_core_wayland::Input* input_ = nullptr;
  tizen_native_window_h egl_window_ = nullptr;
  wl_display* wl2_display_ = nullptr;
  wl_surface* wl2_surface_ = nullptr;
  std::vector<tizen_core_wayland::EventHandler> event_handlers_;
  std::string display_name_;
  uint32_t resource_id_ = 0;
  bool owns_display_ = false;
  bool owns_window_ = false;

#ifdef TV_PROFILE
  bool pointing_device_support_ = true;
  bool floating_menu_support_ = true;
  bool show_unsupported_toast_ = false;
#endif
  bool is_vulkan_ = false;
};

}  // namespace flutter

#endif  // EMBEDDER_TIZEN_WINDOW_ECORE_WL2_H_
