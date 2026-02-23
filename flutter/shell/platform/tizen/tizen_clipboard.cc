// Copyright 2024 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tizen_clipboard.h"

#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <utility>

#include "flutter/shell/platform/tizen/logger.h"
#include "flutter/shell/platform/tizen/tizen_window_ecore_wl2.h"

namespace flutter {

namespace {

constexpr char kMimeTypeTextPlain[] = "text/plain;charset=utf-8";

}  // namespace

TizenClipboard::TizenClipboard(TizenViewBase* view) {
  if (!InitializeFromWindow(view)) {
    InitializeFallbackDisplay();
  }

  if (!InitializeDataDevice()) {
    FT_LOG(Error) << "Failed to initialize clipboard data device.";
  }
}

TizenClipboard::~TizenClipboard() {
  on_data_callback_ = nullptr;

  if (data_source_) {
    wl_data_source_destroy(data_source_);
    data_source_ = nullptr;
  }

  for (auto& entry : offer_mime_types_) {
    if (entry.first) {
      wl_data_offer_destroy(entry.first);
    }
  }
  offer_mime_types_.clear();
  selection_offer_ = nullptr;

  if (data_device_) {
    wl_data_device_destroy(data_device_);
    data_device_ = nullptr;
  }

  if (data_device_manager_) {
    wl_data_device_manager_destroy(data_device_manager_);
    data_device_manager_ = nullptr;
  }

  if (seat_ && owns_display_) {
    wl_seat_destroy(seat_);
    seat_ = nullptr;
  }

  if (registry_ && owns_display_) {
    wl_registry_destroy(registry_);
    registry_ = nullptr;
  }

  if (display_ && owns_display_) {
    wl_display_disconnect(display_);
    display_ = nullptr;
  }
}

void TizenClipboard::SetData(const std::string& data) {
  data_ = data;

  if (!data_device_manager_ || !data_device_) {
    FT_LOG(Error) << "Clipboard data source is unavailable.";
    return;
  }

  if (data_source_) {
    wl_data_source_destroy(data_source_);
    data_source_ = nullptr;
  }

  data_source_ = wl_data_device_manager_create_data_source(data_device_manager_);
  if (!data_source_) {
    FT_LOG(Error) << "Failed to create wl_data_source.";
    return;
  }

  static const wl_data_source_listener kSourceListener = {
      HandleDataSourceTarget,
      HandleDataSourceSend,
      HandleDataSourceCancelled,
      HandleDataSourceDndDropPerformed,
      HandleDataSourceDndFinished,
      HandleDataSourceAction,
  };
  wl_data_source_add_listener(data_source_, &kSourceListener, this);

  wl_data_source_offer(data_source_, kMimeTypeTextPlain);

  uint32_t serial = window_ ? window_->GetLastInputSerial() : 0;
  wl_data_device_set_selection(data_device_, data_source_, serial);
  wl_display_flush(display_);
}

bool TizenClipboard::GetData(ClipboardCallback on_data_callback) {
  on_data_callback_ = std::move(on_data_callback);

  if (!selection_offer_) {
    FT_LOG(Info) << "No clipboard selection offer.";
    if (on_data_callback_) {
      on_data_callback_("");
      on_data_callback_ = nullptr;
    }
    return false;
  }

  std::optional<std::string> data = ReceiveStringData();
  if (on_data_callback_) {
    on_data_callback_(data);
    on_data_callback_ = nullptr;
  }
  return data.has_value();
}

bool TizenClipboard::HasStrings() {
  return IsTextMimeTypeAvailable();
}

void TizenClipboard::HandleRegistryGlobal(void* data,
                                          wl_registry* registry,
                                          uint32_t name,
                                          const char* interface,
                                          uint32_t version) {
  auto* self = static_cast<TizenClipboard*>(data);
  if (!self) {
    return;
  }

  if (strcmp(interface, wl_seat_interface.name) == 0 && !self->seat_) {
    self->seat_ = static_cast<wl_seat*>(
        wl_registry_bind(registry, name, &wl_seat_interface,
                         std::min(version, 5u)));
  } else if (strcmp(interface, wl_data_device_manager_interface.name) == 0 &&
             !self->data_device_manager_) {
    self->data_device_manager_ = static_cast<wl_data_device_manager*>(
        wl_registry_bind(registry, name, &wl_data_device_manager_interface,
                         std::min(version, 3u)));
  }
}

void TizenClipboard::HandleRegistryGlobalRemove(void* data,
                                                wl_registry* registry,
                                                uint32_t name) {}

void TizenClipboard::HandleDataOffer(void* data,
                                     wl_data_device* data_device,
                                     wl_data_offer* offer) {
  auto* self = static_cast<TizenClipboard*>(data);
  if (!self || !offer) {
    return;
  }

  static const wl_data_offer_listener kOfferListener = {
      HandleOfferMimeType,
      HandleOfferSourceActions,
      HandleOfferAction,
  };
  wl_data_offer_add_listener(offer, &kOfferListener, self);
  self->offer_mime_types_[offer] = {};
}

void TizenClipboard::HandleDataEnter(void* data,
                                     wl_data_device* data_device,
                                     uint32_t serial,
                                     wl_surface* surface,
                                     wl_fixed_t x,
                                     wl_fixed_t y,
                                     wl_data_offer* offer) {}

void TizenClipboard::HandleDataLeave(void* data, wl_data_device* data_device) {}

void TizenClipboard::HandleDataMotion(void* data,
                                      wl_data_device* data_device,
                                      uint32_t time,
                                      wl_fixed_t x,
                                      wl_fixed_t y) {}

void TizenClipboard::HandleDataDrop(void* data, wl_data_device* data_device) {}

void TizenClipboard::HandleSelection(void* data,
                                     wl_data_device* data_device,
                                     wl_data_offer* offer) {
  auto* self = static_cast<TizenClipboard*>(data);
  if (!self) {
    return;
  }

  if (self->selection_offer_ && self->selection_offer_ != offer) {
    auto iter = self->offer_mime_types_.find(self->selection_offer_);
    if (iter != self->offer_mime_types_.end()) {
      wl_data_offer_destroy(iter->first);
      self->offer_mime_types_.erase(iter);
    }
  }

  self->selection_offer_ = offer;
}

void TizenClipboard::HandleOfferMimeType(void* data,
                                         wl_data_offer* offer,
                                         const char* mime_type) {
  auto* self = static_cast<TizenClipboard*>(data);
  if (!self || !offer || !mime_type) {
    return;
  }

  auto iter = self->offer_mime_types_.find(offer);
  if (iter == self->offer_mime_types_.end()) {
    return;
  }
  iter->second.emplace_back(mime_type);
}

void TizenClipboard::HandleOfferSourceActions(void* data,
                                              wl_data_offer* offer,
                                              uint32_t source_actions) {}

void TizenClipboard::HandleOfferAction(void* data,
                                       wl_data_offer* offer,
                                       uint32_t dnd_action) {}

void TizenClipboard::HandleDataSourceTarget(void* data,
                                            wl_data_source* source,
                                            const char* mime_type) {}

void TizenClipboard::HandleDataSourceSend(void* data,
                                          wl_data_source* source,
                                          const char* mime_type,
                                          int32_t fd) {
  auto* self = static_cast<TizenClipboard*>(data);
  if (!self || fd < 0) {
    if (fd >= 0) {
      close(fd);
    }
    return;
  }

  if (mime_type && strcmp(mime_type, kMimeTypeTextPlain) != 0) {
    FT_LOG(Error) << "Invalid mime type(" << mime_type << ").";
    close(fd);
    return;
  }

  if (!self->data_.empty()) {
    ssize_t write_result =
        write(fd, self->data_.data(), static_cast<size_t>(self->data_.size()));
    if (write_result < 0) {
      FT_LOG(Error) << "Failed to write clipboard data.";
    }
  }
  close(fd);
}

void TizenClipboard::HandleDataSourceCancelled(void* data,
                                               wl_data_source* source) {
  auto* self = static_cast<TizenClipboard*>(data);
  if (!self || !source) {
    return;
  }

  if (self->data_source_ == source) {
    wl_data_source_destroy(source);
    self->data_source_ = nullptr;
  }
}

void TizenClipboard::HandleDataSourceDndDropPerformed(void* data,
                                                      wl_data_source* source) {}

void TizenClipboard::HandleDataSourceDndFinished(void* data,
                                                 wl_data_source* source) {}

void TizenClipboard::HandleDataSourceAction(void* data,
                                            wl_data_source* source,
                                            uint32_t dnd_action) {}

bool TizenClipboard::InitializeFromWindow(TizenViewBase* view) {
  auto* window = dynamic_cast<TizenWindowEcoreWl2*>(view);
  if (!window) {
    return false;
  }

  window_ = window;
  display_ = window->GetDisplay();
  seat_ = window->GetSeat();
  data_device_manager_ = window->GetDataDeviceManager();

  if (!display_ || !seat_ || !data_device_manager_) {
    return false;
  }

  return true;
}

bool TizenClipboard::InitializeFallbackDisplay() {
  display_ = wl_display_connect(nullptr);
  if (!display_) {
    FT_LOG(Error) << "Failed to connect Wayland display for clipboard.";
    return false;
  }

  owns_display_ = true;

  registry_ = wl_display_get_registry(display_);
  if (!registry_) {
    FT_LOG(Error) << "Failed to get Wayland registry for clipboard.";
    return false;
  }

  static const wl_registry_listener kRegistryListener = {
      HandleRegistryGlobal,
      HandleRegistryGlobalRemove,
  };
  wl_registry_add_listener(registry_, &kRegistryListener, this);

  wl_display_roundtrip(display_);
  wl_display_roundtrip(display_);

  if (!seat_ || !data_device_manager_) {
    FT_LOG(Error) << "Missing clipboard globals from Wayland registry.";
    return false;
  }

  return true;
}

bool TizenClipboard::InitializeDataDevice() {
  if (!display_ || !seat_ || !data_device_manager_) {
    return false;
  }

  data_device_ = wl_data_device_manager_get_data_device(data_device_manager_, seat_);
  if (!data_device_) {
    return false;
  }

  static const wl_data_device_listener kDeviceListener = {
      HandleDataOffer,
      HandleDataEnter,
      HandleDataLeave,
      HandleDataMotion,
      HandleDataDrop,
      HandleSelection,
  };
  wl_data_device_add_listener(data_device_, &kDeviceListener, this);

  wl_display_roundtrip(display_);
  return true;
}

bool TizenClipboard::IsTextMimeTypeAvailable() const {
  if (!selection_offer_) {
    return false;
  }

  auto iter = offer_mime_types_.find(selection_offer_);
  if (iter == offer_mime_types_.end()) {
    return false;
  }

  for (const auto& mime_type : iter->second) {
    if (mime_type == kMimeTypeTextPlain) {
      return true;
    }
  }

  return false;
}

std::optional<std::string> TizenClipboard::ReceiveStringData() {
  if (!selection_offer_) {
    return std::nullopt;
  }

  int fd[2] = {-1, -1};
  if (pipe(fd) != 0) {
    FT_LOG(Error) << "Failed to create pipe for clipboard read.";
    return std::nullopt;
  }

  wl_data_offer_receive(selection_offer_, kMimeTypeTextPlain, fd[1]);
  wl_display_flush(display_);
  close(fd[1]);
  fd[1] = -1;

  std::string result;
  std::array<char, 4096> buffer{};

  while (true) {
    ssize_t count = read(fd[0], buffer.data(), buffer.size());
    if (count < 0) {
      FT_LOG(Error) << "Failed to read clipboard data.";
      close(fd[0]);
      return std::nullopt;
    }

    if (count == 0) {
      break;
    }

    result.append(buffer.data(), static_cast<size_t>(count));
  }

  close(fd[0]);
  return result;
}

}  // namespace flutter
