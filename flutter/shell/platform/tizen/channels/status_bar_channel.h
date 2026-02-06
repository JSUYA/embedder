// Copyright 2026 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_TIZEN_CHANNELS_STATUS_BAR_CHANNEL_H_
#define FLUTTER_SHELL_PLATFORM_TIZEN_CHANNELS_STATUS_BAR_CHANNEL_H_

#include <memory>

#include "flutter/shell/platform/common/client_wrapper/include/flutter/method_channel.h"
#include "rapidjson/document.h"

namespace flutter {

// Implements SystemChannels.statusBar (OptionalMethodChannel 'flutter/status_bar').
//
// On iOS, the platform sends `handleScrollToTop` when the user taps the status
// bar. Tizen currently does not generate this gesture, but we provide the
// channel wiring so that platform code can emit the event in the future.
class StatusBarChannel {
 public:
  explicit StatusBarChannel(BinaryMessenger* messenger);
  virtual ~StatusBarChannel();

  // Emits the iOS-style scroll-to-top event to the framework.
  void SendScrollToTop();

 private:
  std::unique_ptr<MethodChannel<rapidjson::Document>> channel_;
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_TIZEN_CHANNELS_STATUS_BAR_CHANNEL_H_
