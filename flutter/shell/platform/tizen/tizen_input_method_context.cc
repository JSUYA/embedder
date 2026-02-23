// Copyright 2021 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tizen_input_method_context.h"

#include <algorithm>

#include <text-client-protocol.h>
#include <wayland-client-protocol.h>

#include "flutter/shell/platform/tizen/logger.h"

namespace {

constexpr uint32_t kInputPanelEventTypeState = 0;

}  // namespace

namespace flutter {

TizenInputMethodContext::TizenInputMethodContext(
    uintptr_t window_id,
    wl_display* display,
    wl_seat* seat,
    wl_surface* surface,
    wl_text_input_manager* text_input_manager)
    : display_(display),
      seat_(seat),
      surface_(surface) {
  InitializeTextInput(text_input_manager);
}

TizenInputMethodContext::~TizenInputMethodContext() {
  UnregisterInputPanelEventCallback();

  if (text_input_) {
    wl_text_input_destroy(text_input_);
    text_input_ = nullptr;
  }
}

bool TizenInputMethodContext::HandleKeyEvent(const char* device_name,
                                             uint32_t device_class,
                                             uint32_t device_subclass,
                                             const char* key,
                                             const char* string,
                                             uint32_t modifiers,
                                             uint32_t scan_code,
                                             size_t timestamp,
                                             bool is_down) {
  if (!text_input_ || !input_panel_shown_) {
    return false;
  }

  const char* key_name = key ? key : "";
  pending_filter_result_ = false;
  pending_filter_serial_ = ++serial_;
  wl_text_input_filter_key_event(
      text_input_, pending_filter_serial_, static_cast<uint32_t>(timestamp),
      key_name,
      is_down ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED,
      modifiers, device_name ? device_name : "", device_class, device_subclass,
      scan_code);
  CommitState();

  // This can synchronously dispatch if a filter result is already queued,
  // while remaining non-blocking for normal key-path latency.
  if (display_) {
    wl_display_dispatch_pending(display_);
  }

  if (pending_filter_serial_ == 0) {
    return pending_filter_result_;
  }
  return false;
}

InputPanelGeometry TizenInputMethodContext::GetInputPanelGeometry() {
  return input_panel_geometry_;
}

void TizenInputMethodContext::ResetInputMethodContext() {
  if (!text_input_) {
    return;
  }
  wl_text_input_reset(text_input_);
  CommitState();
}

void TizenInputMethodContext::ShowInputPanel() {
  if (!text_input_) {
    input_panel_shown_ = true;
    NotifyInputPanelState("show");
    return;
  }

  if (seat_ && surface_) {
    wl_text_input_activate(text_input_, seat_, surface_);
  }
  wl_text_input_show_input_panel(text_input_);
  wl_text_input_input_panel_enabled(text_input_, 1);
  CommitState();
}

void TizenInputMethodContext::HideInputPanel() {
  if (!text_input_) {
    input_panel_shown_ = false;
    NotifyInputPanelState("hide");
    return;
  }

  wl_text_input_hide_input_panel(text_input_);
  if (seat_) {
    wl_text_input_deactivate(text_input_, seat_);
  }
  CommitState();
}

bool TizenInputMethodContext::IsInputPanelShown() {
  return input_panel_shown_;
}

void TizenInputMethodContext::SetInputPanelLayout(const std::string& input_type) {
  input_type_ = input_type;
  ApplyContentType();
}

void TizenInputMethodContext::SetInputPanelLayoutVariation(bool is_signed,
                                                           bool is_decimal) {
  number_signed_ = is_signed;
  number_decimal_ = is_decimal;
  ApplyContentType();
}

void TizenInputMethodContext::SetAutocapitalType(const std::string& type) {
  text_capitalization_ = type;

  if (!text_input_) {
    return;
  }

  if (type == "TextCapitalization.characters") {
    wl_text_input_set_capital_mode(text_input_,
                                   WL_TEXT_INPUT_CAPITAL_MODE_UPPERCASE);
  } else {
    wl_text_input_set_capital_mode(text_input_,
                                   WL_TEXT_INPUT_CAPITAL_MODE_LOWERCASE);
  }
  CommitState();
}

void TizenInputMethodContext::RegisterInputPanelEventCallback() {
  // Input panel events are always registered through wl_text_input_listener.
}

void TizenInputMethodContext::UnregisterInputPanelEventCallback() {
  // Input panel events are always registered through wl_text_input_listener.
}

void TizenInputMethodContext::InitializeTextInput(
    wl_text_input_manager* text_input_manager) {
  if (!display_ || !seat_ || !surface_ || !text_input_manager) {
    FT_LOG(Info)
        << "wl_text_input is unavailable. Falling back to local text state.";
    return;
  }

  text_input_ = wl_text_input_manager_create_text_input(text_input_manager);
  if (!text_input_) {
    FT_LOG(Error) << "Failed to create wl_text_input.";
    return;
  }

  RegisterTextInputListener();
  SetAutocapitalType(text_capitalization_);
  ApplyContentType();
}

void TizenInputMethodContext::RegisterTextInputListener() {
  if (!text_input_) {
    return;
  }

  static const wl_text_input_listener kTextInputListener = {
      EnterCallback,
      LeaveCallback,
      nullptr,  // modifiers_map
      InputPanelStateCallback,
      PreeditStringCallback,
      nullptr,  // preedit_styling
      PreeditCursorCallback,
      CommitStringCallback,
      nullptr,  // cursor_position
      nullptr,  // delete_surrounding_text
      nullptr,  // keysym
      nullptr,  // language
      nullptr,  // text_direction
      nullptr,  // selection_region
      nullptr,  // private_command
      InputPanelGeometryCallback,
      nullptr,  // input_panel_data
      nullptr,  // get_selection_text
      nullptr,  // get_surrounding_text
      FilterKeyEventDoneCallback,
      nullptr,  // hide_permission
      nullptr,  // recapture_string
      InputPanelEventCallback,
      nullptr,  // commit_content
  };

  wl_text_input_add_listener(text_input_, &kTextInputListener, this);
}

void TizenInputMethodContext::CommitState() {
  if (!text_input_) {
    return;
  }

  wl_text_input_commit_state(text_input_, ++serial_);
  if (display_) {
    wl_display_flush(display_);
  }
}

void TizenInputMethodContext::NotifyInputPanelState(const std::string& state) {
  if (on_input_panel_state_changed_) {
    on_input_panel_state_changed_(state);
  }
}

void TizenInputMethodContext::ApplyContentType() {
  if (!text_input_) {
    return;
  }

  uint32_t purpose = WL_TEXT_INPUT_CONTENT_PURPOSE_NORMAL;
  uint32_t hint = WL_TEXT_INPUT_CONTENT_HINT_DEFAULT;

  if (input_type_ == "TextInputType.multiline") {
    purpose = WL_TEXT_INPUT_CONTENT_PURPOSE_NORMAL;
    hint |= WL_TEXT_INPUT_CONTENT_HINT_MULTILINE;
  } else if (input_type_ == "TextInputType.number") {
    if (number_signed_ && number_decimal_) {
      purpose = WL_TEXT_INPUT_CONTENT_PURPOSE_DIGITS_SIGNEDDECIMAL;
    } else if (number_signed_) {
      purpose = WL_TEXT_INPUT_CONTENT_PURPOSE_DIGITS_SIGNED;
    } else if (number_decimal_) {
      purpose = WL_TEXT_INPUT_CONTENT_PURPOSE_DIGITS_DECIMAL;
    } else {
      purpose = WL_TEXT_INPUT_CONTENT_PURPOSE_DIGITS;
    }
  } else if (input_type_ == "TextInputType.phone") {
    purpose = WL_TEXT_INPUT_CONTENT_PURPOSE_PHONE;
  } else if (input_type_ == "TextInputType.datetime") {
    purpose = WL_TEXT_INPUT_CONTENT_PURPOSE_DATETIME;
  } else if (input_type_ == "TextInputType.emailAddress" ||
             input_type_ == "TextInputType.twitter") {
    purpose = WL_TEXT_INPUT_CONTENT_PURPOSE_EMAIL;
  } else if (input_type_ == "TextInputType.url" ||
             input_type_ == "TextInputType.webSearch") {
    purpose = WL_TEXT_INPUT_CONTENT_PURPOSE_URL;
  } else if (input_type_ == "TextInputType.visiblePassword") {
    purpose = WL_TEXT_INPUT_CONTENT_PURPOSE_PASSWORD;
    hint = WL_TEXT_INPUT_CONTENT_HINT_PASSWORD;
  }

  wl_text_input_set_content_type(text_input_, hint, purpose);
  CommitState();
}

void TizenInputMethodContext::EnterCallback(void* data,
                                            wl_text_input* text_input,
                                            wl_surface* surface) {
  auto* self = static_cast<TizenInputMethodContext*>(data);
  if (!self) {
    return;
  }
  self->input_panel_shown_ = true;
}

void TizenInputMethodContext::LeaveCallback(void* data,
                                            wl_text_input* text_input) {
  auto* self = static_cast<TizenInputMethodContext*>(data);
  if (!self) {
    return;
  }

  self->input_panel_shown_ = false;
  if (self->preediting_) {
    self->preediting_ = false;
    if (self->on_preedit_end_) {
      self->on_preedit_end_();
    }
  }
}

void TizenInputMethodContext::InputPanelStateCallback(void* data,
                                                      wl_text_input* text_input,
                                                      uint32_t state) {
  auto* self = static_cast<TizenInputMethodContext*>(data);
  if (!self) {
    return;
  }

  switch (state) {
    case WL_TEXT_INPUT_INPUT_PANEL_STATE_SHOW:
      self->input_panel_shown_ = true;
      self->NotifyInputPanelState("show");
      break;
    case WL_TEXT_INPUT_INPUT_PANEL_STATE_HIDE:
    default:
      self->input_panel_shown_ = false;
      self->NotifyInputPanelState("hide");
      break;
  }
}

void TizenInputMethodContext::PreeditStringCallback(void* data,
                                                    wl_text_input* text_input,
                                                    uint32_t serial,
                                                    const char* text,
                                                    const char* commit) {
  auto* self = static_cast<TizenInputMethodContext*>(data);
  if (!self) {
    return;
  }

  if (!self->preediting_) {
    self->preediting_ = true;
    if (self->on_preedit_start_) {
      self->on_preedit_start_();
    }
  }

  if (self->on_preedit_changed_) {
    self->on_preedit_changed_(text ? text : "", self->preedit_cursor_pos_);
  }
}

void TizenInputMethodContext::PreeditCursorCallback(void* data,
                                                    wl_text_input* text_input,
                                                    int32_t index) {
  auto* self = static_cast<TizenInputMethodContext*>(data);
  if (!self) {
    return;
  }
  self->preedit_cursor_pos_ = index;
}

void TizenInputMethodContext::CommitStringCallback(void* data,
                                                   wl_text_input* text_input,
                                                   uint32_t serial,
                                                   const char* text) {
  auto* self = static_cast<TizenInputMethodContext*>(data);
  if (!self) {
    return;
  }

  if (self->preediting_) {
    self->preediting_ = false;
    if (self->on_preedit_end_) {
      self->on_preedit_end_();
    }
  }

  if (self->on_commit_) {
    self->on_commit_(text ? text : "");
  }
}

void TizenInputMethodContext::InputPanelGeometryCallback(
    void* data,
    wl_text_input* text_input,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height) {
  auto* self = static_cast<TizenInputMethodContext*>(data);
  if (!self) {
    return;
  }

  self->input_panel_geometry_.x = x;
  self->input_panel_geometry_.y = y;
  self->input_panel_geometry_.w = width;
  self->input_panel_geometry_.h = height;
}

void TizenInputMethodContext::FilterKeyEventDoneCallback(
    void* data,
    wl_text_input* text_input,
    uint32_t serial,
    uint32_t state) {
  auto* self = static_cast<TizenInputMethodContext*>(data);
  if (!self) {
    return;
  }

  if (self->pending_filter_serial_ == serial) {
    self->pending_filter_result_ = state != 0;
    self->pending_filter_serial_ = 0;
  }
}

void TizenInputMethodContext::InputPanelEventCallback(void* data,
                                                      wl_text_input* text_input,
                                                      uint32_t serial,
                                                      uint32_t event_type,
                                                      uint32_t value) {
  auto* self = static_cast<TizenInputMethodContext*>(data);
  if (!self) {
    return;
  }

  if (event_type != kInputPanelEventTypeState) {
    return;
  }

  switch (value) {
    case WL_TEXT_INPUT_INPUT_PANEL_STATE_SHOW:
      self->input_panel_shown_ = true;
      self->NotifyInputPanelState("show");
      break;
    case WL_TEXT_INPUT_INPUT_PANEL_STATE_HIDE:
      self->input_panel_shown_ = false;
      self->NotifyInputPanelState("hide");
      break;
    default:
      self->NotifyInputPanelState("unknown");
      break;
  }
}

}  // namespace flutter
