// Copyright 2026 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "public/flutter_tizen.h"

#include <dlfcn.h>

#include <memory>
#include <string>

#include "flutter/shell/platform/tizen/flutter_tizen_engine.h"
#include "flutter/shell/platform/tizen/flutter_tizen_nui_bridge.h"
#include "flutter/shell/platform/tizen/flutter_tizen_view.h"
#include "flutter/shell/platform/tizen/logger.h"
#include "flutter/shell/platform/tizen/tizen_view_base.h"

namespace {

using FlutterDesktopViewCreateFromImageViewNuiFn = FlutterDesktopViewRef (*)(
    const FlutterDesktopViewProperties& view_properties,
    FlutterDesktopEngineRef engine,
    void* image_view,
    void* native_image_queue,
    int32_t default_window_id,
    const FlutterTizenNuiBridge* bridge);

constexpr char kNuiCreateSymbol[] = "FlutterDesktopViewCreateFromImageViewNui";

std::string GetNuiLibraryPath() {
  Dl_info info = {};
  if (dladdr(reinterpret_cast<void*>(&GetNuiLibraryPath), &info) == 0 ||
      !info.dli_fname) {
    return FLUTTER_TIZEN_NUI_LIBRARY;
  }

  std::string path = info.dli_fname;
  size_t separator = path.find_last_of('/');
  if (separator == std::string::npos) {
    return FLUTTER_TIZEN_NUI_LIBRARY;
  }
  return path.substr(0, separator + 1) + FLUTTER_TIZEN_NUI_LIBRARY;
}

FlutterDesktopViewCreateFromImageViewNuiFn ResolveNuiCreateFunction() {
  static void* library = []() -> void* {
    std::string library_path = GetNuiLibraryPath();
    void* handle = dlopen(library_path.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
      flutter::FT_LOG(Error) << "Failed to load NUI helper library "
                             << library_path << ": " << dlerror();
    }
    return handle;
  }();

  if (!library) {
    return nullptr;
  }

  static auto create_from_image_view =
      reinterpret_cast<FlutterDesktopViewCreateFromImageViewNuiFn>(
          dlsym(library, kNuiCreateSymbol));
  if (!create_from_image_view) {
    flutter::FT_LOG(Error) << "Failed to resolve " << kNuiCreateSymbol
                           << " from " << FLUTTER_TIZEN_NUI_LIBRARY << ": "
                           << dlerror();
  }
  return create_from_image_view;
}

flutter::FlutterTizenEngine* EngineFromHandle(FlutterDesktopEngineRef ref) {
  return reinterpret_cast<flutter::FlutterTizenEngine*>(ref);
}

FlutterDesktopViewRef HandleForView(flutter::FlutterTizenView* view) {
  return reinterpret_cast<FlutterDesktopViewRef>(view);
}

FlutterDesktopViewRef CreateViewFromTizenView(
    const FlutterDesktopViewProperties& view_properties,
    FlutterDesktopEngineRef engine,
    flutter::TizenViewBase* tizen_view) {
  (void)view_properties;

  auto view = std::make_unique<flutter::FlutterTizenView>(
      flutter::kImplicitViewId,
      std::unique_ptr<flutter::TizenViewBase>(tizen_view),
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

const FlutterTizenNuiBridge* GetNuiBridge() {
  static const FlutterTizenNuiBridge bridge = {
      CreateViewFromTizenView,
  };
  return &bridge;
}

}  // namespace

FlutterDesktopViewRef FlutterDesktopViewCreateFromImageView(
    const FlutterDesktopViewProperties& view_properties,
    FlutterDesktopEngineRef engine,
    void* image_view,
    void* native_image_queue,
    int32_t default_window_id) {
  auto create_from_image_view = ResolveNuiCreateFunction();
  if (!create_from_image_view) {
    return nullptr;
  }
  return create_from_image_view(view_properties, engine, image_view,
                                native_image_queue, default_window_id,
                                GetNuiBridge());
}
