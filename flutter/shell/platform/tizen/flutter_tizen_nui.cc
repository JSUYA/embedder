// Copyright 2022 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/flutter_tizen.h"

#include <memory>

#include "flutter/shell/platform/tizen/flutter_tizen_engine.h"
#include "flutter/shell/platform/tizen/flutter_tizen_view.h"
#include "flutter/shell/platform/tizen/logger.h"
#include "flutter/shell/platform/tizen/tizen_nui_backend_loader.h"
#include "flutter/shell/platform/tizen/tizen_view_nui.h"

namespace {

// Returns the engine corresponding to the given opaque API handle.
flutter::FlutterTizenEngine* EngineFromHandle(FlutterDesktopEngineRef ref) {
  return reinterpret_cast<flutter::FlutterTizenEngine*>(ref);
}

FlutterDesktopViewRef HandleForView(flutter::FlutterTizenView* view) {
  return reinterpret_cast<FlutterDesktopViewRef>(view);
}

}  // namespace

FlutterDesktopViewRef FlutterDesktopViewCreateFromImageView(
    const FlutterDesktopViewProperties& view_properties,
    FlutterDesktopEngineRef engine,
    void* image_view,
    void* native_image_queue,
    int32_t default_window_id) {
  // Validate that the NUI/DALi backend and its dependencies are available
  // before touching any DALi object. This is the only place DALi is required,
  // so an app that never calls this function never needs DALi at all.
  if (!flutter::GetTizenNuiBackend()) {
    FT_LOG(Error) << "Cannot create a view from an image view because the "
                     "NUI/DALi backend is unavailable. Ensure "
                     "libflutter_tizen_nui.so and its DALi dependencies are "
                     "installed on the device.";
    return nullptr;
  }

  std::unique_ptr<flutter::TizenViewBase> tizen_view =
      std::make_unique<flutter::TizenViewNui>(
          view_properties.width, view_properties.height, image_view,
          native_image_queue, default_window_id);

  auto view = std::make_unique<flutter::FlutterTizenView>(
      flutter::kImplicitViewId, std::move(tizen_view),
      std::unique_ptr<flutter::FlutterTizenEngine>(EngineFromHandle(engine)),
      FlutterDesktopRendererType::kEGL);

  if (!view->engine()->IsRunning()) {
    if (!view->engine()->RunEngine()) {
      return nullptr;
    }
  }

  view->SendInitialGeometry();

  return HandleForView(view.release());
}
