// Copyright 2022 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_TIZEN_VIEW_NUI_H_
#define EMBEDDER_TIZEN_VIEW_NUI_H_

#include <cstdint>
#include <string>

#include "flutter/shell/platform/tizen/tizen_nui_backend.h"
#include "flutter/shell/platform/tizen/tizen_view.h"

namespace flutter {

class TizenViewNui : public TizenView {
 public:
  // |image_view| must be a Dali::Toolkit::ImageView* and |native_image_queue|
  // must be a Dali::NativeImageSourceQueue*. They are held as opaque handles
  // and only manipulated through the NUI backend vtable, so this class carries
  // no DALi link-time dependency.
  TizenViewNui(int32_t width,
               int32_t height,
               void* image_view,
               void* native_image_queue,
               int32_t default_window_id);

  ~TizenViewNui();

  TizenGeometry GetGeometry() override;

  bool SetGeometry(TizenGeometry geometry) override;

  void* GetRenderTarget() override { return native_image_queue_; }

  void* GetNativeHandle() override { return image_view_; }

  int32_t GetDpi() override;

  uintptr_t GetWindowId() override;

  uint32_t GetResourceId() override;

  void Show() override;

  void RequestRendering();

  void OnKey(const char* device_name,
             uint32_t device_class,
             uint32_t device_subclass,
             const char* key,
             const char* string,
             const char* compose,
             uint32_t modifiers,
             uint32_t scan_code,
             size_t timestamp,
             bool is_down);

  void UpdateFlutterCursor(const std::string& kind) override;

 private:
  void RegisterEventHandlers();

  void UnregisterEventHandlers();

  void PrepareInputMethod();

  void RenderOnce();

  static void RenderOnceThunk(void* user_data);

  const FlutterTizenNuiBackend* backend_ = nullptr;
  void* image_view_ = nullptr;
  void* native_image_queue_ = nullptr;
  int32_t default_window_id_;
  void* rendering_callback_ = nullptr;
};

}  // namespace flutter

#endif  // EMBEDDER_TIZEN_VIEW_NUI_H_
