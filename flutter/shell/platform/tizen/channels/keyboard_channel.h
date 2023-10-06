// Copyright 2023 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_KEYBOARD_CHANNEL_H_
#define EMBEDDER_KEYBOARD_CHANNEL_H_

#include "flutter/shell/platform/common/client_wrapper/include/flutter/binary_messenger.h"
#include "flutter/shell/platform/common/client_wrapper/include/flutter/encodable_value.h"
#include "flutter/shell/platform/common/client_wrapper/include/flutter/method_channel.h"

namespace flutter {

// Channel to set the system's cursor type.
class KeyboardChannel {
 public:
  explicit KeyboardChannel(BinaryMessenger* messenger);
  virtual ~KeyboardChannel();

  void SendKeyboardState(int32_t physical_key, int32_t logical_key);

 private:
  // Called when a method is called on |channel_|;
  void HandleMethodCall(
      const flutter::MethodCall<EncodableValue>& method_call,
      std::unique_ptr<flutter::MethodResult<EncodableValue>> result);

  // The MethodChannel used for communication with the Flutter engine.
  std::unique_ptr<flutter::MethodChannel<EncodableValue>> channel_;

  std::map<int32_t, int32_t> keyboard_state_records_;
};

}  // namespace flutter

#endif  // EMBEDDER_KEYBOARD_CHANNEL_H_
