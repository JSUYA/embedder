// Copyright 2026 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/tizen/ecore_wl2_context.h"

#include "flutter/shell/platform/tizen/logger.h"

namespace flutter {

namespace {

// The process-wide instance holder. A weak_ptr is used so that the context is
// torn down once the last outstanding shared_ptr is released, matching the
// behaviour of the previous per-window init/shutdown pairing.
std::weak_ptr<EcoreWl2Context> g_instance;
std::mutex g_instance_mutex;

}  // namespace

std::shared_ptr<EcoreWl2Context> EcoreWl2Context::Acquire() {
  std::lock_guard<std::mutex> lock(g_instance_mutex);

  if (auto existing = g_instance.lock()) {
    return existing;
  }

  // Use new + shared_ptr with a custom reset pattern rather than
  // make_shared so that the constructor stays private.
  std::shared_ptr<EcoreWl2Context> context(new EcoreWl2Context());
  if (!context->Init()) {
    FT_LOG(Error) << "Failed to initialize EcoreWl2Context.";
    return nullptr;
  }

  g_instance = context;
  return context;
}

EcoreWl2Context::~EcoreWl2Context() {
  // Reverse the initialization order: disconnect the display before
  // shutting down the library. Both calls are safe to make even if the
  // corresponding init step failed partway through, because we guard each
  // with its own non-null / initialized check.
  if (display_) {
    ecore_wl2_display_disconnect(display_);
    display_ = nullptr;
  }
  wl_display_ = nullptr;

  if (initialized_) {
    ecore_wl2_shutdown();
    initialized_ = false;
  }
}

bool EcoreWl2Context::Init() {
  if (!ecore_wl2_init()) {
    FT_LOG(Error) << "Could not initialize Ecore Wl2.";
    return false;
  }
  initialized_ = true;

  display_ = ecore_wl2_display_connect(nullptr);
  if (!display_) {
    FT_LOG(Error) << "Ecore Wl2 display not found.";
    return false;
  }
  wl_display_ = ecore_wl2_display_get(display_);
  if (!wl_display_) {
    FT_LOG(Error) << "Failed to retrieve native wl_display from Ecore_Wl2.";
    return false;
  }

  // Flush protocol once so that registry globals (tizen_policy, tizen_surface,
  // tizen_cursor) are advertised before the first window is created. Without
  // this, the first caller of ecore_wl2_display_globals_get() may see an
  // empty iterator. Preserving the original behaviour from
  // TizenWindowEcoreWl2::CreateWindow().
  ecore_wl2_sync();

  return true;
}

void EcoreWl2Context::GetScreenSize(int32_t* width, int32_t* height) {
  if (!display_) {
    if (width) {
      *width = 0;
    }
    if (height) {
      *height = 0;
    }
    return;
  }
  ecore_wl2_display_screen_size_get(display_, width, height);
}

}  // namespace flutter
