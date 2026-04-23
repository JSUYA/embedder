// Copyright 2026 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_ECORE_WL2_CONTEXT_H_
#define EMBEDDER_ECORE_WL2_CONTEXT_H_

#define EFL_BETA_API_SUPPORT
#include <Ecore_Wl2.h>
#include <tizen-extension-client-protocol.h>

#include <cstdint>
#include <memory>
#include <mutex>

namespace flutter {

// Process-wide refcounted context that owns the Ecore_Wl2 initialization and
// the single Wayland display connection shared by all Tizen windows.
//
// Prior to multi-view support, each TizenWindowEcoreWl2 instance called
// ecore_wl2_init() and ecore_wl2_display_connect(nullptr) in its constructor
// and matched shutdown/disconnect in its destructor. That coupling made it
// impossible to have more than one window alive at the same time, because
// destroying the first window tore down the global state relied upon by any
// subsequent windows.
//
// EcoreWl2Context decouples these lifetimes by holding the global Wayland
// resources in a singleton shared_ptr. The first Acquire() call performs the
// initialization; subsequent calls return the same instance. The resources are
// released only when the last outstanding shared_ptr is dropped.
//
// Thread-safety: Acquire() is thread-safe. All other operations must be called
// from the platform (main) thread, since Ecore_Wl2 and the underlying Wayland
// client library are not thread-safe.
class EcoreWl2Context {
 public:
  // Returns the process-wide instance, creating and initializing it on the
  // first call. Returns nullptr on initialization failure.
  static std::shared_ptr<EcoreWl2Context> Acquire();

  ~EcoreWl2Context();

  EcoreWl2Context(const EcoreWl2Context&) = delete;
  EcoreWl2Context& operator=(const EcoreWl2Context&) = delete;

  Ecore_Wl2_Display* display() const { return display_; }
  wl_display* wl_display_native() const { return wl_display_; }

  // Returns true if the context is valid and ready to create windows.
  bool IsValid() const { return display_ != nullptr && wl_display_ != nullptr; }

  // Returns the screen size reported by the display. Writes 0 on failure.
  void GetScreenSize(int32_t* width, int32_t* height);

 private:
  EcoreWl2Context() = default;
  bool Init();

  Ecore_Wl2_Display* display_ = nullptr;
  wl_display* wl_display_ = nullptr;
  bool initialized_ = false;
};

}  // namespace flutter

#endif  // EMBEDDER_ECORE_WL2_CONTEXT_H_
