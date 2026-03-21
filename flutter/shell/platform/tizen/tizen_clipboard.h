// Copyright 2024 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_TIZEN_CLIPBOARD_H_
#define EMBEDDER_TIZEN_CLIPBOARD_H_

#include <tizen_core_wayland.h>

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "flutter/shell/platform/tizen/tizen_view_base.h"

namespace flutter {

class TizenClipboard {
 public:
  using ClipboardCallback =
      std::function<void(std::optional<std::string> data)>;

  TizenClipboard(TizenViewBase* view);
  virtual ~TizenClipboard();

  bool IsAvailable() const { return input_ != nullptr && display_ != nullptr; }
  void SetData(const std::string& data);
  bool GetData(ClipboardCallback on_data_callback);
  bool HasStrings();

 private:
  void SendData(const tizen_core_wayland::DataSourceSendEvent* event);
  void ReceiveData(const tizen_core_wayland::OfferDataReadyEvent* event);

  std::string data_;
  ClipboardCallback on_data_callback_;
  uint32_t selection_serial_ = 0;
  tizen_core_wayland::Offer* selection_offer_ = nullptr;
  tizen_core_wayland::Display* display_ = nullptr;
  tizen_core_wayland::Input* input_ = nullptr;
  std::vector<tizen_core_wayland::EventHandler> event_handlers_;
};

}  // namespace flutter

#endif  // EMBEDDER_TIZEN_CLIPBOARD_H_
