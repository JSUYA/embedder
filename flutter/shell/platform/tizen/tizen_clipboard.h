// Copyright 2024 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_TIZEN_CLIPBOARD_H_
#define EMBEDDER_TIZEN_CLIPBOARD_H_

#include <wayland-client-protocol.h>

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "flutter/shell/platform/tizen/tizen_view_base.h"

namespace flutter {

class TizenWindowEcoreWl2;

class TizenClipboard {
 public:
  using ClipboardCallback =
      std::function<void(std::optional<std::string> data)>;

  explicit TizenClipboard(TizenViewBase* view);
  virtual ~TizenClipboard();

  void SetData(const std::string& data);
  bool GetData(ClipboardCallback on_data_callback);
  bool HasStrings();

 private:
  static void HandleRegistryGlobal(void* data,
                                   wl_registry* registry,
                                   uint32_t name,
                                   const char* interface,
                                   uint32_t version);
  static void HandleRegistryGlobalRemove(void* data,
                                         wl_registry* registry,
                                         uint32_t name);

  static void HandleDataOffer(void* data,
                              wl_data_device* data_device,
                              wl_data_offer* offer);
  static void HandleDataEnter(void* data,
                              wl_data_device* data_device,
                              uint32_t serial,
                              wl_surface* surface,
                              wl_fixed_t x,
                              wl_fixed_t y,
                              wl_data_offer* offer);
  static void HandleDataLeave(void* data, wl_data_device* data_device);
  static void HandleDataMotion(void* data,
                               wl_data_device* data_device,
                               uint32_t time,
                               wl_fixed_t x,
                               wl_fixed_t y);
  static void HandleDataDrop(void* data, wl_data_device* data_device);
  static void HandleSelection(void* data,
                              wl_data_device* data_device,
                              wl_data_offer* offer);

  static void HandleOfferMimeType(void* data,
                                  wl_data_offer* offer,
                                  const char* mime_type);
  static void HandleOfferSourceActions(void* data,
                                       wl_data_offer* offer,
                                       uint32_t source_actions);
  static void HandleOfferAction(void* data,
                                wl_data_offer* offer,
                                uint32_t dnd_action);

  static void HandleDataSourceTarget(void* data,
                                     wl_data_source* source,
                                     const char* mime_type);
  static void HandleDataSourceSend(void* data,
                                   wl_data_source* source,
                                   const char* mime_type,
                                   int32_t fd);
  static void HandleDataSourceCancelled(void* data, wl_data_source* source);
  static void HandleDataSourceDndDropPerformed(void* data,
                                               wl_data_source* source);
  static void HandleDataSourceDndFinished(void* data,
                                          wl_data_source* source);
  static void HandleDataSourceAction(void* data,
                                     wl_data_source* source,
                                     uint32_t dnd_action);

  bool InitializeFromWindow(TizenViewBase* view);
  bool InitializeFallbackDisplay();
  bool InitializeDataDevice();
  bool IsTextMimeTypeAvailable() const;
  std::optional<std::string> ReceiveStringData();

  std::string data_;
  ClipboardCallback on_data_callback_;

  TizenWindowEcoreWl2* window_ = nullptr;

  wl_display* display_ = nullptr;
  wl_registry* registry_ = nullptr;
  wl_seat* seat_ = nullptr;
  wl_data_device_manager* data_device_manager_ = nullptr;
  wl_data_device* data_device_ = nullptr;
  wl_data_offer* selection_offer_ = nullptr;
  wl_data_source* data_source_ = nullptr;

  bool owns_display_ = false;
  std::unordered_map<wl_data_offer*, std::vector<std::string>> offer_mime_types_;
};

}  // namespace flutter

#endif  // EMBEDDER_TIZEN_CLIPBOARD_H_
