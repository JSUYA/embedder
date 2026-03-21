// Copyright 2021 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tizen_input_method_context.h"

#include <cstdlib>

#include "flutter/shell/platform/tizen/logger.h"

namespace {

tizen_core_imf_input_panel_layout_e TextInputTypeToInputPanelLayout(
    const std::string& text_input_type) {
  if (text_input_type == "TextInputType.text" ||
      text_input_type == "TextInputType.multiline") {
    return TIZEN_CORE_IMF_INPUT_PANEL_LAYOUT_NORMAL;
  } else if (text_input_type == "TextInputType.number") {
    return TIZEN_CORE_IMF_INPUT_PANEL_LAYOUT_NUMBERONLY;
  } else if (text_input_type == "TextInputType.phone") {
    return TIZEN_CORE_IMF_INPUT_PANEL_LAYOUT_PHONENUMBER;
  } else if (text_input_type == "TextInputType.datetime") {
    return TIZEN_CORE_IMF_INPUT_PANEL_LAYOUT_DATETIME;
  } else if (text_input_type == "TextInputType.emailAddress" ||
             text_input_type == "TextInputType.twitter") {
    return TIZEN_CORE_IMF_INPUT_PANEL_LAYOUT_EMAIL;
  } else if (text_input_type == "TextInputType.url" ||
             text_input_type == "TextInputType.webSearch") {
    return TIZEN_CORE_IMF_INPUT_PANEL_LAYOUT_URL;
  } else if (text_input_type == "TextInputType.visiblePassword") {
    return TIZEN_CORE_IMF_INPUT_PANEL_LAYOUT_PASSWORD;
  } else {
    FT_LOG(Warn) << "The requested input type " << text_input_type
                 << " is not supported.";
    return TIZEN_CORE_IMF_INPUT_PANEL_LAYOUT_NORMAL;
  }
}

tizen_core_imf_keyboard_modifiers_e ToImfModifiers(uint32_t modifiers) {
  uint32_t result = TIZEN_CORE_IMF_KEYBOARD_MODIFIERS_NONE;
  if (modifiers & 0x0002) {
    result |= TIZEN_CORE_IMF_KEYBOARD_MODIFIERS_CTRL;
  }
  if (modifiers & 0x0004) {
    result |= TIZEN_CORE_IMF_KEYBOARD_MODIFIERS_ALT;
  }
  if (modifiers & 0x0001) {
    result |= TIZEN_CORE_IMF_KEYBOARD_MODIFIERS_SHIFT;
  }
  if (modifiers & 0x0008) {
    result |= TIZEN_CORE_IMF_KEYBOARD_MODIFIERS_WIN;
  }
  if (modifiers & 0x0400) {
    result |= TIZEN_CORE_IMF_KEYBOARD_MODIFIERS_ALTGR;
  }
  return static_cast<tizen_core_imf_keyboard_modifiers_e>(result);
}

tizen_core_imf_keyboard_locks_e ToImfLocks(uint32_t modifiers) {
  uint32_t result = TIZEN_CORE_IMF_KEYBOARD_LOCKS_NONE;
  if (modifiers & (0x0020 | 0x0100)) {
    result |= TIZEN_CORE_IMF_KEYBOARD_LOCKS_NUM;
  }
  if (modifiers & (0x0040 | 0x0200)) {
    result |= TIZEN_CORE_IMF_KEYBOARD_LOCKS_CAPS;
  }
  if (modifiers & (0x0010 | 0x0080)) {
    result |= TIZEN_CORE_IMF_KEYBOARD_LOCKS_SCROLL;
  }
  return static_cast<tizen_core_imf_keyboard_locks_e>(result);
}

tizen_core_imf_device_class_e ToImfDeviceClass(uint32_t device_class) {
  switch (device_class) {
    case 1:
      return TIZEN_CORE_IMF_DEVICE_CLASS_SEAT;
    case 2:
      return TIZEN_CORE_IMF_DEVICE_CLASS_KEYBOARD;
    case 3:
      return TIZEN_CORE_IMF_DEVICE_CLASS_MOUSE;
    case 4:
    case 5:
      return TIZEN_CORE_IMF_DEVICE_CLASS_TOUCH;
    default:
      return TIZEN_CORE_IMF_DEVICE_CLASS_NONE;
  }
}

tizen_core_imf_device_subclass_e ToImfDeviceSubclass(uint32_t device_subclass) {
  switch (device_subclass) {
    case 12:
      return TIZEN_CORE_IMF_DEVICE_SUBCLASS_VIRTUAL_KEYBOARD;
    case 11:
      return TIZEN_CORE_IMF_DEVICE_SUBCLASS_REMOCON;
    default:
      return TIZEN_CORE_IMF_DEVICE_SUBCLASS_NONE;
  }
}

}  // namespace

namespace flutter {

TizenInputMethodContext::TizenInputMethodContext(void* client_window) {
  if (tizen_core_imf_init() != TIZEN_CORE_IMF_ERROR_NONE) {
    FT_LOG(Error) << "Failed to initialize tizen-core-imf.";
    return;
  }

  if (tizen_core_imf_context_create(&imf_context_) !=
      TIZEN_CORE_IMF_ERROR_NONE) {
    FT_LOG(Error) << "Failed to create tizen-core-imf context.";
    tizen_core_imf_shutdown();
    return;
  }

  if (tizen_core_imf_context_set_client_window(imf_context_, client_window) !=
      TIZEN_CORE_IMF_ERROR_NONE) {
    FT_LOG(Warn) << "Failed to set the IMF client window.";
  }

  SetContextOptions();
  SetInputPanelOptions();
  RegisterEventCallbacks();
  RegisterInputPanelEventCallback();
}

TizenInputMethodContext::~TizenInputMethodContext() {
  if (!imf_context_) {
    tizen_core_imf_shutdown();
    return;
  }

  UnregisterInputPanelEventCallback();
  UnregisterEventCallbacks();
  tizen_core_imf_context_destroy(imf_context_);
  imf_context_ = nullptr;
  tizen_core_imf_shutdown();
}

bool TizenInputMethodContext::HandleKeyEvent(const char* device_name,
                                             uint32_t device_class,
                                             uint32_t device_subclass,
                                             const char* key,
                                             const char* key_name,
                                             const char* string,
                                             const char* compose,
                                             uint32_t modifiers,
                                             uint32_t scan_code,
                                             size_t timestamp,
                                             bool is_down) {
  return HandleKeyEventInternal(device_name, device_class, device_subclass, key,
                                key_name, string, compose, modifiers,
                                scan_code, timestamp, is_down);
}

#ifdef NUI_SUPPORT
bool TizenInputMethodContext::HandleNuiKeyEvent(const char* device_name,
                                                uint32_t device_class,
                                                uint32_t device_subclass,
                                                const char* key,
                                                const char* string,
                                                uint32_t modifiers,
                                                uint32_t scan_code,
                                                size_t timestamp,
                                                bool is_down) {
  return HandleKeyEventInternal(device_name, device_class, device_subclass, key,
                                key, string, "", modifiers, scan_code,
                                timestamp, is_down);
}
#endif

InputPanelGeometry TizenInputMethodContext::GetInputPanelGeometry() {
  InputPanelGeometry geometry;
  if (!imf_context_) {
    return geometry;
  }

  tizen_core_imf_context_get_input_panel_geometry(
      imf_context_, &geometry.x, &geometry.y, &geometry.w, &geometry.h);
  return geometry;
}

void TizenInputMethodContext::ResetInputMethodContext() {
  if (!imf_context_) {
    return;
  }
  tizen_core_imf_context_reset(imf_context_);
}

void TizenInputMethodContext::ShowInputPanel() {
  if (!imf_context_) {
    return;
  }
  tizen_core_imf_context_input_panel_show(imf_context_);
  tizen_core_imf_context_focus_in(imf_context_);
}

void TizenInputMethodContext::HideInputPanel() {
  if (!imf_context_) {
    return;
  }
  tizen_core_imf_context_focus_out(imf_context_);
  tizen_core_imf_context_input_panel_hide(imf_context_);
}

bool TizenInputMethodContext::IsInputPanelShown() {
  if (!imf_context_) {
    return false;
  }

  tizen_core_imf_input_panel_state_e state =
      TIZEN_CORE_IMF_INPUT_PANEL_STATE_HIDE;
  if (tizen_core_imf_context_get_input_panel_state(imf_context_, &state) !=
      TIZEN_CORE_IMF_ERROR_NONE) {
    return false;
  }
  return state == TIZEN_CORE_IMF_INPUT_PANEL_STATE_SHOW;
}

void TizenInputMethodContext::SetInputPanelLayout(
    const std::string& input_type) {
  if (!imf_context_) {
    return;
  }
  tizen_core_imf_context_set_input_panel_layout(
      imf_context_, TextInputTypeToInputPanelLayout(input_type));
}

void TizenInputMethodContext::SetInputPanelLayoutVariation(bool is_signed,
                                                           bool is_decimal) {
  if (!imf_context_) {
    return;
  }

  int variation = TIZEN_CORE_IMF_LAYOUT_NUMBERONLY_VARIATION_NORMAL;
  if (is_signed && is_decimal) {
    variation = TIZEN_CORE_IMF_LAYOUT_NUMBERONLY_VARIATION_SIGNED_AND_DECIMAL;
  } else if (is_signed) {
    variation = TIZEN_CORE_IMF_LAYOUT_NUMBERONLY_VARIATION_SIGNED;
  } else if (is_decimal) {
    variation = TIZEN_CORE_IMF_LAYOUT_NUMBERONLY_VARIATION_DECIMAL;
  }

  tizen_core_imf_context_set_input_panel_layout_variation(imf_context_,
                                                          variation);
}

void TizenInputMethodContext::SetAutocapitalType(const std::string& type) {
  if (!imf_context_) {
    return;
  }

  tizen_core_imf_autocapital_type_e autocapital =
      TIZEN_CORE_IMF_AUTOCAPITAL_TYPE_NONE;
  if (type == "TextCapitalization.characters") {
    autocapital = TIZEN_CORE_IMF_AUTOCAPITAL_TYPE_ALLCHARACTER;
  } else if (type == "TextCapitalization.words") {
    autocapital = TIZEN_CORE_IMF_AUTOCAPITAL_TYPE_WORD;
  } else if (type == "TextCapitalization.sentences") {
    autocapital = TIZEN_CORE_IMF_AUTOCAPITAL_TYPE_SENTENCE;
  }

  tizen_core_imf_context_set_autocapital_type(imf_context_, autocapital);
}

void TizenInputMethodContext::CommitCallback(tizen_core_imf_context_h ctx,
                                             void* event_info,
                                             void* user_data) {
  auto* self = static_cast<TizenInputMethodContext*>(user_data);
  auto* commit = static_cast<char*>(event_info);
  if (self && self->on_commit_ && commit) {
    self->on_commit_(commit);
  }
}

void TizenInputMethodContext::PreeditStartCallback(
    tizen_core_imf_context_h ctx,
    void* event_info,
    void* user_data) {
  auto* self = static_cast<TizenInputMethodContext*>(user_data);
  if (self && self->on_preedit_start_) {
    self->on_preedit_start_();
  }
}

void TizenInputMethodContext::PreeditEndCallback(tizen_core_imf_context_h ctx,
                                                 void* event_info,
                                                 void* user_data) {
  auto* self = static_cast<TizenInputMethodContext*>(user_data);
  if (self && self->on_preedit_end_) {
    self->on_preedit_end_();
  }
}

void TizenInputMethodContext::PreeditChangedCallback(
    tizen_core_imf_context_h ctx,
    void* event_info,
    void* user_data) {
  auto* self = static_cast<TizenInputMethodContext*>(user_data);
  if (!self || !self->on_preedit_changed_) {
    return;
  }

  char* preedit = nullptr;
  tizen_core_imf_preedit_attr_h* attrs = nullptr;
  int attrs_count = 0;
  int cursor_pos = 0;
  if (tizen_core_imf_context_get_preedit_string(ctx, &preedit, &attrs,
                                                &attrs_count,
                                                &cursor_pos) ==
      TIZEN_CORE_IMF_ERROR_NONE) {
    self->on_preedit_changed_(preedit ? preedit : "", cursor_pos);
  }

  free(preedit);
  if (attrs) {
    tizen_core_imf_preedit_attrs_destroy(attrs, attrs_count);
  }
}

void TizenInputMethodContext::InputPanelStateChangedCallback(
    tizen_core_imf_context_h ctx,
    int value,
    void* user_data) {
  auto* self = static_cast<TizenInputMethodContext*>(user_data);
  if (!self || !self->on_input_panel_state_changed_) {
    return;
  }

  std::string state = "unknown";
  switch (static_cast<tizen_core_imf_input_panel_state_e>(value)) {
    case TIZEN_CORE_IMF_INPUT_PANEL_STATE_SHOW:
      state = "show";
      break;
    case TIZEN_CORE_IMF_INPUT_PANEL_STATE_HIDE:
      state = "hide";
      break;
    case TIZEN_CORE_IMF_INPUT_PANEL_STATE_WILL_SHOW:
      state = "will_show";
      break;
  }
  self->on_input_panel_state_changed_(state);
}

void TizenInputMethodContext::RegisterEventCallbacks() {
  if (!imf_context_) {
    return;
  }

  tizen_core_imf_context_add_event_callback(
      imf_context_, TIZEN_CORE_IMF_CALLBACK_COMMIT, CommitCallback, this);
  tizen_core_imf_context_add_event_callback(
      imf_context_, TIZEN_CORE_IMF_CALLBACK_PREEDIT_START,
      PreeditStartCallback, this);
  tizen_core_imf_context_add_event_callback(
      imf_context_, TIZEN_CORE_IMF_CALLBACK_PREEDIT_END, PreeditEndCallback,
      this);
  tizen_core_imf_context_add_event_callback(
      imf_context_, TIZEN_CORE_IMF_CALLBACK_PREEDIT_CHANGED,
      PreeditChangedCallback, this);
}

void TizenInputMethodContext::UnregisterEventCallbacks() {
  if (!imf_context_) {
    return;
  }
  tizen_core_imf_context_del_event_callback(imf_context_,
                                            TIZEN_CORE_IMF_CALLBACK_COMMIT);
  tizen_core_imf_context_del_event_callback(
      imf_context_, TIZEN_CORE_IMF_CALLBACK_PREEDIT_START);
  tizen_core_imf_context_del_event_callback(
      imf_context_, TIZEN_CORE_IMF_CALLBACK_PREEDIT_END);
  tizen_core_imf_context_del_event_callback(
      imf_context_, TIZEN_CORE_IMF_CALLBACK_PREEDIT_CHANGED);
}

void TizenInputMethodContext::SetContextOptions() {
  if (!imf_context_) {
    return;
  }
  tizen_core_imf_context_set_autocapital_type(
      imf_context_, TIZEN_CORE_IMF_AUTOCAPITAL_TYPE_NONE);
  tizen_core_imf_context_set_prediction_hint(imf_context_, "");
}

void TizenInputMethodContext::SetInputPanelOptions() {
  if (!imf_context_) {
    return;
  }
  tizen_core_imf_context_set_input_panel_layout(
      imf_context_, TIZEN_CORE_IMF_INPUT_PANEL_LAYOUT_NORMAL);
  tizen_core_imf_context_set_input_panel_return_key_type(
      imf_context_, TIZEN_CORE_IMF_INPUT_PANEL_RETURN_KEY_TYPE_DEFAULT);
  tizen_core_imf_context_set_input_panel_enabled(imf_context_, true);
}

bool TizenInputMethodContext::HandleKeyEventInternal(const char* device_name,
                                                     uint32_t device_class,
                                                     uint32_t device_subclass,
                                                     const char* key,
                                                     const char* key_name,
                                                     const char* string,
                                                     const char* compose,
                                                     uint32_t modifiers,
                                                     uint32_t scan_code,
                                                     size_t timestamp,
                                                     bool is_down) {
  if (!imf_context_) {
    return false;
  }

  tizen_core_imf_event_key_h key_event = nullptr;
  if (tizen_core_imf_event_key_create(&key_event) !=
      TIZEN_CORE_IMF_ERROR_NONE) {
    return false;
  }

  tizen_core_imf_event_key_set_keyname(key_event, key_name ? key_name : "");
  tizen_core_imf_event_key_set_key(key_event, key ? key : "");
  tizen_core_imf_event_key_set_string(key_event, string ? string : "");
  tizen_core_imf_event_key_set_compose(key_event, compose ? compose : "");
  tizen_core_imf_event_key_set_timestamp(
      key_event, static_cast<unsigned int>(timestamp));
  tizen_core_imf_event_key_set_device_name(key_event,
                                           device_name ? device_name : "");
  tizen_core_imf_event_key_set_device_class(
      key_event, ToImfDeviceClass(device_class));
  tizen_core_imf_event_key_set_device_subclass(
      key_event, ToImfDeviceSubclass(device_subclass));
  tizen_core_imf_event_key_set_modifiers(key_event, ToImfModifiers(modifiers));
  tizen_core_imf_event_key_set_locks(key_event, ToImfLocks(modifiers));
  tizen_core_imf_event_key_set_keycode(key_event, scan_code);

  bool handled = false;
  tizen_core_imf_context_filter_event(
      imf_context_,
      is_down ? TIZEN_CORE_IMF_EVENT_TYPE_KEY_DOWN
              : TIZEN_CORE_IMF_EVENT_TYPE_KEY_UP,
      key_event, &handled);

  tizen_core_imf_event_key_destroy(key_event);
  return handled;
}

void TizenInputMethodContext::RegisterInputPanelEventCallback() {
  if (!imf_context_) {
    return;
  }
  tizen_core_imf_context_add_input_panel_event_callback(
      imf_context_, TIZEN_CORE_IMF_INPUT_PANEL_EVENT_STATE,
      InputPanelStateChangedCallback, this);
}

void TizenInputMethodContext::UnregisterInputPanelEventCallback() {
  if (!imf_context_) {
    return;
  }
  tizen_core_imf_context_del_input_panel_event_callback(
      imf_context_, TIZEN_CORE_IMF_INPUT_PANEL_EVENT_STATE);
}

}  // namespace flutter
