// Copyright 2021 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_TIZEN_INPUT_METHOD_CONTEXT_H_
#define EMBEDDER_TIZEN_INPUT_METHOD_CONTEXT_H_

#include <tizen_core_imf.h>

#include <functional>
#include <string>

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
  explicit TizenInputMethodContext(void* client_window);
  ~TizenInputMethodContext();

  bool HandleKeyEvent(const char* device_name,
                      uint32_t device_class,
                      uint32_t device_subclass,
                      const char* key,
                      const char* key_name,
                      const char* string,
                      const char* compose,
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
                         bool is_down);
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
  static void CommitCallback(tizen_core_imf_context_h ctx,
                             void* event_info,
                             void* user_data);
  static void PreeditStartCallback(tizen_core_imf_context_h ctx,
                                   void* event_info,
                                   void* user_data);
  static void PreeditEndCallback(tizen_core_imf_context_h ctx,
                                 void* event_info,
                                 void* user_data);
  static void PreeditChangedCallback(tizen_core_imf_context_h ctx,
                                     void* event_info,
                                     void* user_data);
  static void InputPanelStateChangedCallback(tizen_core_imf_context_h ctx,
                                             int value,
                                             void* user_data);

  void RegisterEventCallbacks();
  void UnregisterEventCallbacks();

  void SetContextOptions();
  void SetInputPanelOptions();
  bool HandleKeyEventInternal(const char* device_name,
                              uint32_t device_class,
                              uint32_t device_subclass,
                              const char* key,
                              const char* key_name,
                              const char* string,
                              const char* compose,
                              uint32_t modifiers,
                              uint32_t scan_code,
                              size_t timestamp,
                              bool is_down);

  tizen_core_imf_context_h imf_context_ = nullptr;
  OnCommit on_commit_;
  OnPreeditChanged on_preedit_changed_;
  OnPreeditStart on_preedit_start_;
  OnPreeditEnd on_preedit_end_;
  OnInputPanelStateChanged on_input_panel_state_changed_;
};

}  // namespace flutter

#endif  // EMBEDDER_TIZEN_INPUT_METHOD_CONTEXT_H_
