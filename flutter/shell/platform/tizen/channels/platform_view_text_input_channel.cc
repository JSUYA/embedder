// Copyright 2026 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform_view_text_input_channel.h"

#include "flutter/shell/platform/common/client_wrapper/include/flutter/standard_method_codec.h"
#include "flutter/shell/platform/tizen/channels/encodable_value_holder.h"
#include "flutter/shell/platform/tizen/logger.h"
#include "flutter/shell/platform/tizen/tizen_input_method_context.h"

namespace flutter {

namespace {

constexpr char kChannelName[] = "tizen/internal/platformviewtextinput";

constexpr char kBadArgumentError[] = "Bad Arguments";

}  // namespace

PlatformViewTextInputChannel::PlatformViewTextInputChannel(
    BinaryMessenger* messenger,
    TizenInputMethodContext* imf_context)
    : channel_(std::make_unique<MethodChannel<EncodableValue>>(
          messenger,
          kChannelName,
          &StandardMethodCodec::GetInstance())),
      imf_context_(imf_context) {
  channel_->SetMethodCallHandler(
      [this](const MethodCall<EncodableValue>& call,
             std::unique_ptr<MethodResult<EncodableValue>> result) {
        HandleMethodCall(call, std::move(result));
      });
}

PlatformViewTextInputChannel::~PlatformViewTextInputChannel() {}

void PlatformViewTextInputChannel::HandleMethodCall(
    const MethodCall<EncodableValue>& method_call,
    std::unique_ptr<MethodResult<EncodableValue>> result) {
  const std::string& method = method_call.method_name();
  const auto* arguments =
      std::get_if<EncodableMap>(method_call.arguments());
  if (!arguments) {
    result->Error(kBadArgumentError, "Method invoked without args.");
    return;
  }
  EncodableValueHolder<int32_t> view_id(arguments, "viewId");
  if (!view_id) {
    result->Error(kBadArgumentError, "viewId is required.");
    return;
  }

  if (method == "activate") {
    active_view_id_ = *view_id;
    imf_context_->SetInputPanelEnabled(true);
    imf_context_->ShowInputPanel();
    imf_context_->SetEditingActive(
        TizenInputMethodContext::EditingSource::kPlatformView, true);
    result->Success();
  } else if (method == "deactivate") {
    if (*view_id == active_view_id_) {
      Deactivate();
    }
    result->Success();
  } else {
    FT_LOG(Warn) << "Unimplemented method: " << method;
    result->NotImplemented();
  }
}

void PlatformViewTextInputChannel::Deactivate() {
  if (imf_context_->editing_source() !=
      TizenInputMethodContext::EditingSource::kPlatformView) {
    // Another client (e.g. a Flutter text field) has taken over editing;
    // do not tear down its session.
    active_view_id_ = -1;
    return;
  }
  // Resetting may flush a pending commit to the view, so reset while the
  // view ID is still valid, then make sure the plugin closes any ongoing
  // composition.
  imf_context_->ResetInputMethodContext();
  OnComposeEnd();
  imf_context_->SetEditingActive(
      TizenInputMethodContext::EditingSource::kPlatformView, false);
  imf_context_->HideInputPanel();
  active_view_id_ = -1;
}

void PlatformViewTextInputChannel::OnComposeBegin() {
  EncodableMap args;
  args[EncodableValue("viewId")] = EncodableValue(active_view_id_);
  channel_->InvokeMethod("preeditStart",
                         std::make_unique<EncodableValue>(args));
}

void PlatformViewTextInputChannel::OnComposeChange(const std::string& str,
                                                   int cursor_pos) {
  EncodableMap args;
  args[EncodableValue("viewId")] = EncodableValue(active_view_id_);
  args[EncodableValue("text")] = EncodableValue(str);
  args[EncodableValue("cursor")] = EncodableValue(cursor_pos);
  channel_->InvokeMethod("preeditChanged",
                         std::make_unique<EncodableValue>(args));
}

void PlatformViewTextInputChannel::OnComposeEnd() {
  EncodableMap args;
  args[EncodableValue("viewId")] = EncodableValue(active_view_id_);
  channel_->InvokeMethod("preeditEnd", std::make_unique<EncodableValue>(args));
}

void PlatformViewTextInputChannel::OnCommit(const std::string& str) {
  EncodableMap args;
  args[EncodableValue("viewId")] = EncodableValue(active_view_id_);
  args[EncodableValue("text")] = EncodableValue(str);
  channel_->InvokeMethod("commit", std::make_unique<EncodableValue>(args));
}

}  // namespace flutter
