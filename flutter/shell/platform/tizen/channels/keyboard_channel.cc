// Copyright 2023 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "keyboard_channel.h"

#include "flutter/shell/platform/common/client_wrapper/include/flutter/standard_method_codec.h"
#include "flutter/shell/platform/tizen/logger.h"

namespace flutter {

namespace {

constexpr char kChannelName[] = "flutter/keyboard";
constexpr char kGetKeyboardStateMethod[] = "getKeyboardState";

}  // namespace

KeyboardChannel::KeyboardChannel(BinaryMessenger* messenger) {
  channel_ = std::make_unique<MethodChannel<EncodableValue>>(
      messenger, kChannelName, &StandardMethodCodec::GetInstance());
  channel_->SetMethodCallHandler(
      [this](const MethodCall<EncodableValue>& call,
             std::unique_ptr<MethodResult<EncodableValue>> result) {
        HandleMethodCall(call, std::move(result));
      });
}

KeyboardChannel::~KeyboardChannel() {}

void KeyboardChannel::HandleMethodCall(
    const MethodCall<EncodableValue>& method_call,
    std::unique_ptr<MethodResult<EncodableValue>> result) {
  const std::string& method_name = method_call.method_name();
  if (method_name == kGetKeyboardStateMethod) {
    EncodableMap map;
    for (const auto& key : keyboard_state_records_) {
      map[EncodableValue(key.first)] = EncodableValue(key.second);
      keyboard_state_records_.erase(key.first);
    }
    result->Success(EncodableValue(map));
  } else {
    result->NotImplemented();
  }
}

void KeyboardChannel::SendKeyboardState(int32_t physical_key,
                                        int32_t logical_key) {
  keyboard_state_records_[physical_key] = logical_key;
}

}  // namespace flutter
