// Copyright 2026 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/tizen/channels/status_bar_channel.h"

#include "flutter/shell/platform/common/json_method_codec.h"

namespace flutter {

namespace {

constexpr char kChannelName[] = "flutter/status_bar";
constexpr char kHandleScrollToTop[] = "handleScrollToTop";

}  // namespace

StatusBarChannel::StatusBarChannel(BinaryMessenger* messenger)
    : channel_(std::make_unique<MethodChannel<rapidjson::Document>>(
          messenger,
          kChannelName,
          &JsonMethodCodec::GetInstance())) {}

StatusBarChannel::~StatusBarChannel() = default;

void StatusBarChannel::SendScrollToTop() {
  channel_->InvokeMethod(kHandleScrollToTop, nullptr);
}

}  // namespace flutter
