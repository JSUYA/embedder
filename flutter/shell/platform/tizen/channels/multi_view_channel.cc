// Copyright 2026 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/tizen/channels/multi_view_channel.h"

#include <limits>
#include <memory>
#include <optional>
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

// Reads an integer field from an EncodableMap. Dart's `int` is encoded as
// either int32_t or int64_t depending on magnitude, so a naive
// std::get_if<int32_t> on a value that happens to arrive as int64_t (or
// vice versa) would silently return the fallback. EncodableValue provides
// TryGetLongValue() which unifies both variants into int64_t.
int64_t GetInt(const EncodableMap& map, const char* key, int64_t fallback) {
  auto it = map.find(EncodableValue(key));
  if (it == map.end() || it->second.IsNull()) {
    return fallback;
  }
  auto v = it->second.TryGetLongValue();
  return v.has_value() ? *v : fallback;
}

bool GetBool(const EncodableMap& map, const char* key, bool fallback) {
  auto it = map.find(EncodableValue(key));
  if (it == map.end() || it->second.IsNull()) {
    return fallback;
  }
  if (auto* v = std::get_if<bool>(&it->second)) {
    return *v;
  }
  return fallback;
}

double GetDouble(const EncodableMap& map, const char* key, double fallback) {
  auto it = map.find(EncodableValue(key));
  if (it == map.end() || it->second.IsNull()) {
    return fallback;
  }
  if (auto* v = std::get_if<double>(&it->second)) {
    return *v;
  }
  return fallback;
}

bool IsInt32(int64_t value) {
  return value >= std::numeric_limits<int32_t>::min() &&
         value <= std::numeric_limits<int32_t>::max();
}

struct MethodResultHolder {
  MultiViewChannel* channel;
  std::shared_ptr<std::atomic_bool> channel_alive;
  std::unique_ptr<MethodResult<EncodableValue>> result;
};

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
  alive_->store(false);
  if (channel_) {
    channel_->SetMethodCallHandler(nullptr);
  }
  DestroyOwnedViews();
}

void MultiViewChannel::DestroyOwnedViews() {
  auto owned_views = std::move(owned_views_);
  owned_views_.clear();
  FlutterDesktopEngineRef engine_ref =
      reinterpret_cast<FlutterDesktopEngineRef>(engine_);
  for (const auto& [view_id, view] : owned_views) {
    if (engine_ && engine_->IsRunning()) {
      FlutterDesktopEngineRemoveView(engine_ref, view_id, nullptr, nullptr);
    } else {
      FlutterDesktopViewDestroy(view);
    }
  }
}

void MultiViewChannel::OnAddViewComplete(
    bool added,
    FlutterDesktopViewId view_id,
    std::unique_ptr<MethodResult<EncodableValue>> result) {
  if (!added) {
    result->Error("add-view-failed",
                  "FlutterEngineAddView rejected the new view.");
    return;
  }
  FlutterDesktopEngineRef engine_ref =
      reinterpret_cast<FlutterDesktopEngineRef>(engine_);
  FlutterDesktopViewRef view = FlutterDesktopEngineGetView(engine_ref, view_id);
  if (!view) {
    result->Error("add-view-failed",
                  "FlutterEngineAddView completed without a native view.");
    return;
  }
  owned_views_[view_id] = view;
  result->Success(EncodableValue(static_cast<int64_t>(view_id)));
}

void MultiViewChannel::OnRemoveViewComplete(
    bool removed,
    FlutterDesktopViewId view_id,
    std::unique_ptr<MethodResult<EncodableValue>> result) {
  if (removed) {
    owned_views_.erase(view_id);
  }
  result->Success(EncodableValue(removed));
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
    const int64_t x_value = GetInt(*args, "x", 0);
    const int64_t y_value = GetInt(*args, "y", 0);
    const int64_t width_value = GetInt(*args, "width", 0);
    const int64_t height_value = GetInt(*args, "height", 0);
    if (!IsInt32(x_value) || !IsInt32(y_value) || !IsInt32(width_value) ||
        !IsInt32(height_value)) {
      result->Error("bad-args",
                    "x, y, width and height must fit in a 32-bit integer.");
      return;
    }
    if (width_value < 0 || height_value < 0) {
      result->Error("bad-args", "width and height must be non-negative.");
      return;
    }
    const int32_t x = static_cast<int32_t>(x_value);
    const int32_t y = static_cast<int32_t>(y_value);
    const int32_t width = static_cast<int32_t>(width_value);
    const int32_t height = static_cast<int32_t>(height_value);
    const bool transparent = GetBool(*args, "transparent", false);
    const bool top_level = GetBool(*args, "topLevel", false);
    const double user_pixel_ratio = GetDouble(*args, "userPixelRatio", 0.0);
    if (user_pixel_ratio < 0.0) {
      result->Error("bad-args", "userPixelRatio must be non-negative.");
      return;
    }

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

    // The public C API guarantees the callback fires exactly once (even on
    // early failure paths), so the |holder| can be freed unconditionally in
    // the trampoline below without leaking or double-freeing.
    auto* holder = new MethodResultHolder{this, alive_, std::move(result)};
    FlutterDesktopEngineRef engine_ref =
        reinterpret_cast<FlutterDesktopEngineRef>(engine_);
    FlutterDesktopEngineAddView(
        engine_ref, properties,
        [](bool added, FlutterDesktopViewId view_id, void* user_data) {
          std::unique_ptr<MethodResultHolder> holder(
              static_cast<MethodResultHolder*>(user_data));
          if (!holder->channel_alive->load()) {
            return;
          }
          holder->channel->OnAddViewComplete(added, view_id,
                                             std::move(holder->result));
        },
        holder);
    return;
  }

  if (method == "removeView") {
    const int64_t view_id_value = GetInt(*args, "viewId", -1);
    if (view_id_value <= 0) {
      result->Error("bad-args", "viewId must be a positive integer.");
      return;
    }
    const FlutterDesktopViewId view_id =
        static_cast<FlutterDesktopViewId>(view_id_value);
    if (owned_views_.count(view_id) == 0) {
      result->Success(EncodableValue(false));
      return;
    }
    auto* holder = new MethodResultHolder{this, alive_, std::move(result)};
    FlutterDesktopEngineRef engine_ref =
        reinterpret_cast<FlutterDesktopEngineRef>(engine_);
    FlutterDesktopEngineRemoveView(
        engine_ref, view_id,
        [](bool removed, FlutterDesktopViewId view_id, void* user_data) {
          std::unique_ptr<MethodResultHolder> holder(
              static_cast<MethodResultHolder*>(user_data));
          if (!holder->channel_alive->load()) {
            return;
          }
          holder->channel->OnRemoveViewComplete(removed, view_id,
                                                std::move(holder->result));
        },
        holder);
    return;
  }

  result->NotImplemented();
}

}  // namespace flutter
