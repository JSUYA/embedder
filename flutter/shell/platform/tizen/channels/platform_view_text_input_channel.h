// Copyright 2026 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_PLATFORM_VIEW_TEXT_INPUT_CHANNEL_H_
#define EMBEDDER_PLATFORM_VIEW_TEXT_INPUT_CHANNEL_H_

#include <memory>
#include <string>

#include "flutter/shell/platform/common/client_wrapper/include/flutter/binary_messenger.h"
#include "flutter/shell/platform/common/client_wrapper/include/flutter/encodable_value.h"
#include "flutter/shell/platform/common/client_wrapper/include/flutter/method_channel.h"

namespace flutter {

class TizenInputMethodContext;

// A channel that allows platform views (e.g. webviews) to borrow the
// engine's input method context for text composition.
//
// A platform view plugin activates this channel while an editable element
// inside the platform view is focused. Key events are then filtered through
// the IMF, and the resulting preedit and commit strings are forwarded back
// to the plugin through this channel.
class PlatformViewTextInputChannel {
 public:
  explicit PlatformViewTextInputChannel(
      BinaryMessenger* messenger,
      TizenInputMethodContext* imf_context);
  virtual ~PlatformViewTextInputChannel();

  // Whether a platform view is currently editing text.
  bool IsActive() const { return active_view_id_ != -1; }

  void OnComposeBegin();
  void OnComposeChange(const std::string& str, int cursor_pos);
  void OnComposeEnd();
  void OnCommit(const std::string& str);

 private:
  void HandleMethodCall(const MethodCall<EncodableValue>& method_call,
                        std::unique_ptr<MethodResult<EncodableValue>> result);

  void Deactivate();

  std::unique_ptr<MethodChannel<EncodableValue>> channel_;
  TizenInputMethodContext* imf_context_;
  int active_view_id_ = -1;
};

}  // namespace flutter

#endif  // EMBEDDER_PLATFORM_VIEW_TEXT_INPUT_CHANNEL_H_
