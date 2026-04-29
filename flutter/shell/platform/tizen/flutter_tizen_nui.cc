// Copyright 2022 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/flutter_tizen.h"

#include <dali-toolkit/public-api/controls/image-view/image-view.h>
#include <dali/devel-api/adaptor-framework/native-image-source-queue.h>

#include <memory>

#include "flutter/shell/platform/tizen/flutter_tizen_nui_bridge.h"
#include "flutter/shell/platform/tizen/tizen_view_nui.h"

extern "C" FlutterDesktopViewRef FlutterDesktopViewCreateFromImageViewNui(
    const FlutterDesktopViewProperties& view_properties,
    FlutterDesktopEngineRef engine,
    void* image_view,
    void* native_image_queue,
    int32_t default_window_id,
    const FlutterTizenNuiBridge* bridge) {
  if (!bridge || !bridge->create_view) {
    return nullptr;
  }

  std::unique_ptr<flutter::TizenViewBase> tizen_view =
      std::make_unique<flutter::TizenViewNui>(
          view_properties.width, view_properties.height,
          reinterpret_cast<Dali::Toolkit::ImageView*>(image_view),
          reinterpret_cast<Dali::NativeImageSourceQueue*>(native_image_queue),
          default_window_id);

  return bridge->create_view(view_properties, engine, tizen_view.release());
}
