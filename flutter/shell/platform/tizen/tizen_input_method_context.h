// Copyright 2021 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_TIZEN_INPUT_METHOD_CONTEXT_H_
#define EMBEDDER_TIZEN_INPUT_METHOD_CONTEXT_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

struct wl_display;
struct wl_seat;
struct wl_surface;
struct wl_text_input;
struct wl_text_input_manager;

namespace flutter {

using OnCommit = std::function<void(std::string str)>;
using OnPreeditChanged = std::function<void(std::string str, int cursor_pos)>;
using OnPreeditStart = std::function<void()>;
using OnPreeditEnd = std::function<void()>;
using OnInputPanelStateChanged = std::function<void(const std::string& state)>;

struct InputPanelGeometry {
  int32_t x = 0, y = 0, w = 0, h = 0;
};

class TizenInputMethodContext {
 public:
  TizenInputMethodContext(uintptr_t window_id,
                          wl_display* display = nullptr,
                          wl_seat* seat = nullptr,
                          wl_surface* surface = nullptr,
                          wl_text_input_manager* text_input_manager = nullptr);
  ~TizenInputMethodContext();

  bool HandleKeyEvent(const char* device_name,
                      uint32_t device_class,
                      uint32_t device_subclass,
                      const char* key,
                      const char* string,
                      uint32_t modifiers,
                      uint32_t scan_code,
                      size_t timestamp,
                      bool is_down);

#ifdef NUI_SUPPORT
  bool HandleNuiKeyEvent(const char* device_name,
                         uint32_t device_class,
                         uint32_t device_subclass,
                         const char* key,
                         const char* string,
                         uint32_t modifiers,
                         uint32_t scan_code,
                         size_t timestamp,
                         bool is_down) {
    return HandleKeyEvent(device_name, device_class, device_subclass, key,
                          string, modifiers, scan_code, timestamp, is_down);
  }
#endif

  InputPanelGeometry GetInputPanelGeometry();

  void ResetInputMethodContext();

  void ShowInputPanel();

  void HideInputPanel();

  bool IsInputPanelShown();

  void SetInputPanelLayout(const std::string& layout);

  void SetInputPanelLayoutVariation(bool is_signed, bool is_decimal);

  void SetAutocapitalType(const std::string& type);

  void SetOnCommit(OnCommit callback) { on_commit_ = callback; }

  void SetOnPreeditChanged(OnPreeditChanged callback) {
    on_preedit_changed_ = callback;
  }

  void SetOnPreeditStart(OnPreeditStart callback) {
    on_preedit_start_ = callback;
  }

  void SetOnPreeditEnd(OnPreeditEnd callback) { on_preedit_end_ = callback; }

  void SetOnInputPanelStateChanged(OnInputPanelStateChanged callback) {
    on_input_panel_state_changed_ = callback;
  }

  void RegisterInputPanelEventCallback();
  void UnregisterInputPanelEventCallback();

 private:
  static void EnterCallback(void* data,
                            wl_text_input* text_input,
                            wl_surface* surface);
  static void LeaveCallback(void* data, wl_text_input* text_input);
  static void InputPanelStateCallback(void* data,
                                      wl_text_input* text_input,
                                      uint32_t state);
  static void PreeditStringCallback(void* data,
                                    wl_text_input* text_input,
                                    uint32_t serial,
                                    const char* text,
                                    const char* commit);
  static void PreeditCursorCallback(void* data,
                                    wl_text_input* text_input,
                                    int32_t index);
  static void CommitStringCallback(void* data,
                                   wl_text_input* text_input,
                                   uint32_t serial,
                                   const char* text);
  static void InputPanelGeometryCallback(void* data,
                                         wl_text_input* text_input,
                                         uint32_t x,
                                         uint32_t y,
                                         uint32_t width,
                                         uint32_t height);
  static void FilterKeyEventDoneCallback(void* data,
                                         wl_text_input* text_input,
                                         uint32_t serial,
                                         uint32_t state);
  static void InputPanelEventCallback(void* data,
                                      wl_text_input* text_input,
                                      uint32_t serial,
                                      uint32_t event_type,
                                      uint32_t value);
  static void CommitContentCallback(void* data,
                                    wl_text_input* text_input,
                                    uint32_t serial,
                                    const char* content,
                                    const char* description,
                                    const char* mime_types);

  void InitializeTextInput(wl_text_input_manager* text_input_manager);
  void RegisterTextInputListener();
  void CommitState();
  void NotifyInputPanelState(const std::string& state);
  void ApplyContentType();

  wl_display* display_ = nullptr;
  wl_seat* seat_ = nullptr;
  wl_surface* surface_ = nullptr;
  wl_text_input* text_input_ = nullptr;

  bool input_panel_shown_ = false;
  bool preediting_ = false;
  int32_t preedit_cursor_pos_ = 0;
  uint32_t serial_ = 0;
  uint32_t pending_filter_serial_ = 0;
  bool pending_filter_result_ = false;

  std::string input_type_ = "TextInputType.text";
  bool number_signed_ = false;
  bool number_decimal_ = false;
  std::string text_capitalization_ = "TextCapitalization.none";

  InputPanelGeometry input_panel_geometry_;
  OnCommit on_commit_;
  OnPreeditChanged on_preedit_changed_;
  OnPreeditStart on_preedit_start_;
  OnPreeditEnd on_preedit_end_;
  OnInputPanelStateChanged on_input_panel_state_changed_;
};

}  // namespace flutter

#endif  // EMBEDDER_TIZEN_INPUT_METHOD_CONTEXT_H_
