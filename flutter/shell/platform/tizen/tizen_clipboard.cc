// Copyright 2024 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tizen_clipboard.h"

#include <unistd.h>

#include <algorithm>

#include "flutter/shell/platform/tizen/logger.h"
#include "flutter/shell/platform/tizen/tizen_window.h"
#include "flutter/shell/platform/tizen/tizen_window_ecore_wl2.h"

namespace flutter {

namespace {

constexpr char kMimeTypeTextPlain[] = "text/plain;charset=utf-8";

}  // namespace

TizenClipboard::TizenClipboard(TizenViewBase* view) {
  if (auto* window = dynamic_cast<TizenWindowEcoreWl2*>(view)) {
    auto* native_window =
        static_cast<tizen_core_wayland::Window*>(window->GetNativeHandle());
    if (native_window) {
      display_ = native_window->GetDisplay();
    }
  }

  if (!display_) {
    display_ = tizen_core_wayland::DisplayManager::GetInst().Find("");
  }

  if (!display_) {
    FT_LOG(Info) << "Clipboard backend is not available.";
    return;
  }

  input_ = display_->FindDefaultInput();
  if (!input_) {
    FT_LOG(Info) << "Default input is not available for clipboard.";
    return;
  }

  auto& broker = display_->GetEventBroker();
  event_handlers_.push_back(broker.AddListener(
      tizen_core_wayland::EventType::DataSourceSend,
      [this](const tizen_core_wayland::EventBase* event) {
        SendData(
            static_cast<const tizen_core_wayland::DataSourceSendEvent*>(event));
      }));
  event_handlers_.push_back(broker.AddListener(
      tizen_core_wayland::EventType::OfferDataReady,
      [this](const tizen_core_wayland::EventBase* event) {
        ReceiveData(
            static_cast<const tizen_core_wayland::OfferDataReadyEvent*>(event));
      }));
}

TizenClipboard::~TizenClipboard() {
  on_data_callback_ = nullptr;
  if (!display_) {
    return;
  }

  auto& broker = display_->GetEventBroker();
  for (auto handler : event_handlers_) {
    broker.RemoveListener(handler);
  }
  event_handlers_.clear();
}

void TizenClipboard::SendData(
    const tizen_core_wayland::DataSourceSendEvent* event) {
  if (!event) {
    return;
  }

  const std::string& mime_type = event->GetSourceType();
  if (!mime_type.empty() && mime_type != kMimeTypeTextPlain) {
    FT_LOG(Error) << "Invalid mime type("
                  << (mime_type.empty() ? "null" : mime_type.c_str()) << ").";
    if (event->GetFd() >= 0) {
      close(event->GetFd());
    }
    return;
  }

  if (event->GetSerial() != selection_serial_) {
    FT_LOG(Error) << "The serial doesn't match.";
    if (event->GetFd() >= 0) {
      close(event->GetFd());
    }
    return;
  }

  if (event->GetFd() >= 0) {
    write(event->GetFd(), data_.c_str(), data_.length());
    close(event->GetFd());
  }
}

void TizenClipboard::ReceiveData(
    const tizen_core_wayland::OfferDataReadyEvent* event) {
  if (!event) {
    return;
  }

  const auto& data = event->GetData();
  if (data.empty()) {
    FT_LOG(Info) << "No data available.";
    if (on_data_callback_) {
      on_data_callback_("");
      on_data_callback_ = nullptr;
    }
    return;
  }

  if (event->GetOffer() != selection_offer_) {
    FT_LOG(Error) << "The offer doesn't match.";
    if (on_data_callback_) {
      on_data_callback_(std::nullopt);
      on_data_callback_ = nullptr;
    }
    return;
  }

  std::string content(reinterpret_cast<const char*>(data.data()), data.size());

  if (on_data_callback_) {
    on_data_callback_(content);
    on_data_callback_ = nullptr;
  }
}

void TizenClipboard::SetData(const std::string& data) {
  if (!input_ || !display_) {
    return;
  }

  data_ = data;

  selection_serial_ =
      input_->SetDndSelectionType({kMimeTypeTextPlain, std::string()});
  display_->Flush();
}

bool TizenClipboard::GetData(ClipboardCallback on_data_callback) {
  if (!input_) {
    return false;
  }

  on_data_callback_ = std::move(on_data_callback);
  selection_offer_ = input_->GetDndSelectionOffer();

  if (!selection_offer_) {
    FT_LOG(Error) << "GetDndSelectionOffer() failed.";

    if (on_data_callback_) {
      on_data_callback_ = nullptr;
    }
    return false;
  }

  selection_offer_->Receive(kMimeTypeTextPlain);
  return true;
}

bool TizenClipboard::HasStrings() {
  if (!input_) {
    return false;
  }

  selection_offer_ = input_->GetDndSelectionOffer();

  if (!selection_offer_) {
    return false;
  }

  const auto& mime_types = selection_offer_->GetMimeTypes();
  return std::find(mime_types.begin(), mime_types.end(), kMimeTypeTextPlain) !=
         mime_types.end();
}

}  // namespace flutter
