// Copyright 2026 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_MULTI_VIEW_CHANNEL_H_
#define EMBEDDER_MULTI_VIEW_CHANNEL_H_

#include <memory>

#include "flutter/shell/platform/common/client_wrapper/include/flutter/binary_messenger.h"
#include "flutter/shell/platform/common/client_wrapper/include/flutter/encodable_value.h"
#include "flutter/shell/platform/common/client_wrapper/include/flutter/method_channel.h"

namespace flutter {

class FlutterTizenEngine;

// Platform channel handler for "flutter_tizen/multi_view".
//
// Exposes addView / removeView to Dart code via the TizenMultiView Dart
// helper. Both methods operate on the engine that owns this channel.
//
// Method contract:
//   addView({int x, int y, int width, int height, bool transparent,
//            bool topLevel, double userPixelRatio}) -> int (view id)
//   removeView({int viewId}) -> bool
//
// The channel is registered on the engine-level messenger so a single
// instance services calls from every view.
class MultiViewChannel {
 public:
  MultiViewChannel(BinaryMessenger* messenger, FlutterTizenEngine* engine);
  virtual ~MultiViewChannel();

  MultiViewChannel(const MultiViewChannel&) = delete;
  MultiViewChannel& operator=(const MultiViewChannel&) = delete;

 private:
  void HandleMethodCall(const MethodCall<EncodableValue>& method_call,
                        std::unique_ptr<MethodResult<EncodableValue>> result);

  std::unique_ptr<MethodChannel<EncodableValue>> channel_;

  // Non-owning pointer to the engine that created this channel. The engine
  // outlives the channel because the channel is held by the engine itself.
  FlutterTizenEngine* engine_ = nullptr;
};

}  // namespace flutter

#endif  // EMBEDDER_MULTI_VIEW_CHANNEL_H_
