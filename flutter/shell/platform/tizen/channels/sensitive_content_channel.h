// Copyright 2026 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_TIZEN_CHANNELS_SENSITIVE_CONTENT_CHANNEL_H_
#define FLUTTER_SHELL_PLATFORM_TIZEN_CHANNELS_SENSITIVE_CONTENT_CHANNEL_H_

#include <memory>

#include "flutter/shell/platform/common/client_wrapper/include/flutter/method_channel.h"

namespace flutter {

// Implements SystemChannels.sensitiveContent (OptionalMethodChannel
// 'flutter/sensitivecontent').
//
// This channel currently targets Android View content-sensitivity APIs.
// On Tizen we report that the feature is unsupported.
class SensitiveContentChannel {
 public:
  explicit SensitiveContentChannel(BinaryMessenger* messenger);
  virtual ~SensitiveContentChannel();

 private:
  std::unique_ptr<MethodChannel<EncodableValue>> channel_;

  void HandleMethodCall(const MethodCall<EncodableValue>& call,
                        std::unique_ptr<MethodResult<EncodableValue>> result);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_TIZEN_CHANNELS_SENSITIVE_CONTENT_CHANNEL_H_
