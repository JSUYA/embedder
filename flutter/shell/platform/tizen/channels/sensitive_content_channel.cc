// Copyright 2026 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/tizen/channels/sensitive_content_channel.h"

#include <string>

#include "flutter/shell/platform/common/client_wrapper/include/flutter/standard_method_codec.h"

namespace flutter {

namespace {

constexpr char kChannelName[] = "flutter/sensitivecontent";

constexpr char kIsSupportedMethod[] = "isSupported";
constexpr char kGetContentSensitivityMethod[] = "getContentSensitivity";
constexpr char kSetContentSensitivityMethod[] = "setContentSensitivity";

}  // namespace

SensitiveContentChannel::SensitiveContentChannel(BinaryMessenger* messenger)
    : channel_(std::make_unique<MethodChannel<EncodableValue>>(
          messenger,
          kChannelName,
          &StandardMethodCodec::GetInstance())) {
  channel_->SetMethodCallHandler(
      [this](const MethodCall<EncodableValue>& call,
             std::unique_ptr<MethodResult<EncodableValue>> result) {
        HandleMethodCall(call, std::move(result));
      });
}

SensitiveContentChannel::~SensitiveContentChannel() {
  channel_->SetMethodCallHandler(nullptr);
}

void SensitiveContentChannel::HandleMethodCall(
    const MethodCall<EncodableValue>& call,
    std::unique_ptr<MethodResult<EncodableValue>> result) {
  const std::string& method = call.method_name();

  if (method == kIsSupportedMethod) {
    result->Success(EncodableValue(false));
    return;
  }

  // For non-Android platforms, the framework should gate calls via
  // OptionalMethodChannel. Still, handle gracefully.
  if (method == kGetContentSensitivityMethod) {
    // Return a default value (0) to keep callers resilient.
    result->Success(EncodableValue(0));
    return;
  }

  if (method == kSetContentSensitivityMethod) {
    // No-op on Tizen.
    result->Success();
    return;
  }

  result->NotImplemented();
}

}  // namespace flutter
