// Copyright 2020 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_SETTINGS_CHANNEL_H_
#define EMBEDDER_SETTINGS_CHANNEL_H_

#include <Ecore.h>

#include <memory>
#include <mutex>

#include "flutter/shell/platform/common/client_wrapper/include/flutter/basic_message_channel.h"
#include "flutter/shell/platform/common/client_wrapper/include/flutter/binary_messenger.h"
#include "rapidjson/document.h"

namespace flutter {

class SettingsChannel {
 public:
  explicit SettingsChannel(BinaryMessenger* messenger);
  virtual ~SettingsChannel();

 private:
  static Eina_Bool InitialRefresh(void* user_data);
  void RefreshAndSendSettingsEvent();
  void SendSettingsEvent();
  bool UpdatePrefer24HourTime();
  float UpdateTextScaleFactor();

  std::unique_ptr<BasicMessageChannel<rapidjson::Document>> channel_;
  std::mutex mutex_;
  bool prefer_24_hour_time_ = false;
  float text_scale_factor_ = 1.0f;
};

}  // namespace flutter

#endif  // EMBEDDER_SETTINGS_CHANNEL_H_
