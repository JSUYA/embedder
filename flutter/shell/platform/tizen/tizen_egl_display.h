// Copyright 2026 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_TIZEN_EGL_DISPLAY_H_
#define EMBEDDER_TIZEN_EGL_DISPLAY_H_

#include <EGL/egl.h>

#include <memory>
#include <mutex>
#include <string>

namespace flutter {

// Process-wide EGLDisplay wrapper that performs eglInitialize()/eglTerminate()
// exactly once regardless of how many TizenRendererEgl instances exist.
//
// Prior to multi-view support, TizenRendererEgl called eglTerminate() in its
// destructor. That broke any other renderer still attached to the same
// EGLDisplay, because eglTerminate() frees all contexts and surfaces
// associated with the display. TizenEglDisplay moves that lifetime out of the
// per-view renderer so that multiple views can share a single display and
// terminate it only when the last renderer is released.
//
// Thread-safety: Acquire() is thread-safe. Other operations must be invoked
// from the platform (main) thread, since EGL state is typically bound to a
// single thread.
class TizenEglDisplay {
 public:
  // Returns the shared EGLDisplay wrapper, initializing it on the first call.
  //
  // |render_target_display| is passed to eglGetDisplay(). When non-null it is
  // a wl_display*; when null, a tbm_dummy_display is used (the NUI path).
  // |enable_impeller| selects between the Impeller-compatible EGL config
  // (with a 4x MSAA sample buffer and 8-bit stencil) and the Skia-compatible
  // one. Callers must pass the same value for every Acquire() within the
  // process; mixing is not supported because the chosen EGLConfig is shared.
  static std::shared_ptr<TizenEglDisplay> Acquire(void* render_target_display,
                                                  bool enable_impeller);

  ~TizenEglDisplay();

  TizenEglDisplay(const TizenEglDisplay&) = delete;
  TizenEglDisplay& operator=(const TizenEglDisplay&) = delete;

  EGLDisplay egl_display() const { return egl_display_; }
  EGLConfig egl_config() const { return egl_config_; }
  const std::string& extensions() const { return extensions_; }
  bool enable_impeller() const { return enable_impeller_; }
  bool IsValid() const {
    return egl_display_ != EGL_NO_DISPLAY && egl_config_ != nullptr;
  }

 private:
  TizenEglDisplay() = default;
  bool Init(void* render_target_display, bool enable_impeller);
  bool ChooseConfig(bool enable_impeller);

  EGLDisplay egl_display_ = EGL_NO_DISPLAY;
  EGLConfig egl_config_ = nullptr;
  std::string extensions_;
  bool enable_impeller_ = false;
};

}  // namespace flutter

#endif  // EMBEDDER_TIZEN_EGL_DISPLAY_H_
