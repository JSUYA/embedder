// Copyright 2020 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "settings_channel.h"

#include <Ecore.h>
#include <system/system_settings.h>

#include <mutex>

#include "flutter/shell/platform/common/json_message_codec.h"

namespace flutter {

namespace {

constexpr char kChannelName[] = "flutter/settings";

constexpr char kTextScaleFactorKey[] = "textScaleFactor";
constexpr char kAlwaysUse24HourFormatKey[] = "alwaysUse24HourFormat";
constexpr char kPlatformBrightnessKey[] = "platformBrightness";

}  // namespace

SettingsChannel::SettingsChannel(BinaryMessenger* messenger)
    : channel_(std::make_unique<BasicMessageChannel<rapidjson::Document>>(
          messenger,
          kChannelName,
          &JsonMessageCodec::GetInstance())) {
  // Send safe defaults first to avoid startup stalls caused by synchronous
  // system settings reads in the critical launch path.
  SendSettingsEvent();

  system_settings_set_changed_cb(
      SYSTEM_SETTINGS_KEY_LOCALE_TIMEFORMAT_24HOUR,
      [](system_settings_key_e key, void* user_data) -> void {
        (void)key;
        auto* self = static_cast<SettingsChannel*>(user_data);
        {
          std::lock_guard<std::mutex> lock(self->mutex_);
          self->UpdatePrefer24HourTime();
        }
        self->SendSettingsEvent();
      },
      this);

  system_settings_set_changed_cb(
      SYSTEM_SETTINGS_KEY_FONT_SIZE,
      [](system_settings_key_e key, void* user_data) -> void {
        (void)key;
        auto* self = static_cast<SettingsChannel*>(user_data);
        {
          std::lock_guard<std::mutex> lock(self->mutex_);
          self->UpdateTextScaleFactor();
        }
        self->SendSettingsEvent();
      },
      this);

  // Defer the initial synchronous reads until after event-loop startup.
  ecore_timer_add(0.0, &SettingsChannel::InitialRefresh, this);
}

SettingsChannel::~SettingsChannel() {
  system_settings_unset_changed_cb(
      SYSTEM_SETTINGS_KEY_LOCALE_TIMEFORMAT_24HOUR);
  system_settings_unset_changed_cb(SYSTEM_SETTINGS_KEY_FONT_SIZE);
}

Eina_Bool SettingsChannel::InitialRefresh(void* user_data) {
  auto* self = static_cast<SettingsChannel*>(user_data);
  self->RefreshAndSendSettingsEvent();
  return ECORE_CALLBACK_CANCEL;
}

void SettingsChannel::RefreshAndSendSettingsEvent() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    UpdatePrefer24HourTime();
    UpdateTextScaleFactor();
  }
  SendSettingsEvent();
}

void SettingsChannel::SendSettingsEvent() {
  rapidjson::Document event(rapidjson::kObjectType);
  rapidjson::MemoryPoolAllocator<>& allocator = event.GetAllocator();

  float text_scale_factor = 1.0f;
  bool prefer_24_hour_time = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    text_scale_factor = text_scale_factor_;
    prefer_24_hour_time = prefer_24_hour_time_;
  }

  event.AddMember(kTextScaleFactorKey, text_scale_factor, allocator);
  event.AddMember(kAlwaysUse24HourFormatKey, prefer_24_hour_time, allocator);
  event.AddMember(kPlatformBrightnessKey, "light", allocator);
  channel_->Send(event);
}

bool SettingsChannel::UpdatePrefer24HourTime() {
  bool value = false;
  if (system_settings_get_value_bool(
          SYSTEM_SETTINGS_KEY_LOCALE_TIMEFORMAT_24HOUR, &value) ==
      SYSTEM_SETTINGS_ERROR_NONE) {
    prefer_24_hour_time_ = value;
  }
  return prefer_24_hour_time_;
}

float SettingsChannel::UpdateTextScaleFactor() {
  constexpr float small = 0.8f;
  constexpr float normal = 1.0f;
  constexpr float large = 1.2f;
  constexpr float huge = 1.4f;
  constexpr float giant = 1.6f;

  int value = 0;
  if (system_settings_get_value_int(SYSTEM_SETTINGS_KEY_FONT_SIZE, &value) ==
      SYSTEM_SETTINGS_ERROR_NONE) {
    switch (value) {
      case SYSTEM_SETTINGS_FONT_SIZE_SMALL:
        text_scale_factor_ = small;
        break;
      case SYSTEM_SETTINGS_FONT_SIZE_LARGE:
        text_scale_factor_ = large;
        break;
      case SYSTEM_SETTINGS_FONT_SIZE_HUGE:
        text_scale_factor_ = huge;
        break;
      case SYSTEM_SETTINGS_FONT_SIZE_GIANT:
        text_scale_factor_ = giant;
        break;
      default:
        text_scale_factor_ = normal;
        break;
    }
  }

  return text_scale_factor_;
}

}  // namespace flutter
