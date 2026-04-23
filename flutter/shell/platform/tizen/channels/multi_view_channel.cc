// Copyright 2026 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/tizen/channels/multi_view_channel.h"

#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "flutter/shell/platform/common/client_wrapper/include/flutter/standard_method_codec.h"
#include "flutter/shell/platform/tizen/flutter_tizen_engine.h"
#include "flutter/shell/platform/tizen/flutter_tizen_view.h"
#include "flutter/shell/platform/tizen/logger.h"
#include "flutter/shell/platform/tizen/public/flutter_tizen.h"
#include "flutter/shell/platform/tizen/tizen_window_ecore_wl2.h"

namespace flutter {

namespace {

constexpr char kChannelName[] = "flutter_tizen/multi_view";

template <typename T>
T GetOr(const EncodableMap& map, const char* key, T fallback) {
  auto it = map.find(EncodableValue(key));
  if (it == map.end() || it->second.IsNull()) {
    return fallback;
  }
  if (auto* v = std::get_if<T>(&it->second)) {
    return *v;
  }
  return fallback;
}

}  // namespace

MultiViewChannel::MultiViewChannel(BinaryMessenger* messenger,
                                   FlutterTizenEngine* engine)
    : engine_(engine) {
  channel_ = std::make_unique<MethodChannel<EncodableValue>>(
      messenger, kChannelName, &StandardMethodCodec::GetInstance());
  channel_->SetMethodCallHandler(
      [this](const MethodCall<EncodableValue>& call,
             std::unique_ptr<MethodResult<EncodableValue>> result) {
        HandleMethodCall(call, std::move(result));
      });
}

MultiViewChannel::~MultiViewChannel() {
  if (channel_) {
    channel_->SetMethodCallHandler(nullptr);
  }
}

void MultiViewChannel::HandleMethodCall(
    const MethodCall<EncodableValue>& method_call,
    std::unique_ptr<MethodResult<EncodableValue>> result) {
  const std::string& method = method_call.method_name();
  const auto* args = std::get_if<EncodableMap>(method_call.arguments());
  if (!args) {
    result->Error("bad-args", "Expected a map of arguments.");
    return;
  }

  if (!engine_ || !engine_->IsRunning()) {
    result->Error("engine-not-running",
                  "Multi-view operations require a running engine.");
    return;
  }

  if (method == "addView") {
    const int32_t x = GetOr<int32_t>(*args, "x", 0);
    const int32_t y = GetOr<int32_t>(*args, "y", 0);
    const int32_t width = GetOr<int32_t>(*args, "width", 0);
    const int32_t height = GetOr<int32_t>(*args, "height", 0);
    const bool transparent = GetOr<bool>(*args, "transparent", false);
    const bool top_level = GetOr<bool>(*args, "topLevel", false);
    const double user_pixel_ratio =
        GetOr<double>(*args, "userPixelRatio", 0.0);

    FlutterDesktopWindowProperties properties = {};
    properties.x = x;
    properties.y = y;
    properties.width = width;
    properties.height = height;
    properties.transparent = transparent;
    properties.focusable = true;
    properties.top_level = top_level;
    properties.renderer_type = kEGL;
    properties.user_pixel_ratio = user_pixel_ratio;
    properties.window_handle = nullptr;
    properties.pointing_device_support = true;
    properties.floating_menu_support = true;

    // Shared ownership of the result so the async callback can outlive the
    // method call. MethodResult is not copyable, so we wrap it.
    auto shared_result =
        std::shared_ptr<MethodResult<EncodableValue>>(std::move(result));

    FlutterDesktopEngineRef engine_ref =
        reinterpret_cast<FlutterDesktopEngineRef>(engine_);
    FlutterDesktopViewRef created = FlutterDesktopEngineAddView(
        engine_ref, properties,
        [](bool added, FlutterDesktopViewId view_id, void* user_data) {
          auto* holder =
              static_cast<std::shared_ptr<MethodResult<EncodableValue>>*>(
                  user_data);
          if (added) {
            (*holder)->Success(EncodableValue(static_cast<int64_t>(view_id)));
          } else {
            (*holder)->Error("add-view-failed",
                             "FlutterEngineAddView rejected the new view.");
          }
          delete holder;
        },
        new std::shared_ptr<MethodResult<EncodableValue>>(shared_result));
    if (!created) {
      shared_result->Error("add-view-issue-failed",
                           "Could not issue FlutterDesktopEngineAddView.");
    }
    return;
  }

  if (method == "removeView") {
    const int64_t view_id = GetOr<int64_t>(*args, "viewId", -1);
    if (view_id <= 0) {
      result->Error("bad-args", "viewId must be a positive integer.");
      return;
    }

    auto shared_result =
        std::shared_ptr<MethodResult<EncodableValue>>(std::move(result));
    FlutterDesktopEngineRef engine_ref =
        reinterpret_cast<FlutterDesktopEngineRef>(engine_);
    const bool issued = FlutterDesktopEngineRemoveView(
        engine_ref, static_cast<FlutterDesktopViewId>(view_id),
        [](bool removed, FlutterDesktopViewId view_id, void* user_data) {
          auto* holder =
              static_cast<std::shared_ptr<MethodResult<EncodableValue>>*>(
                  user_data);
          (*holder)->Success(EncodableValue(removed));
          delete holder;
        },
        new std::shared_ptr<MethodResult<EncodableValue>>(shared_result));
    if (!issued) {
      shared_result->Error("remove-view-issue-failed",
                           "Could not issue FlutterDesktopEngineRemoveView.");
    }
    return;
  }

  result->NotImplemented();
}

}  // namespace flutter
