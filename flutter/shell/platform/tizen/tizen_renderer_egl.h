// Copyright 2020 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_TIZEN_RENDERER_EGL_H_
#define EMBEDDER_TIZEN_RENDERER_EGL_H_

#include <EGL/egl.h>

#include <deque>
#include <string>
#include <vector>

#include "flutter/shell/platform/tizen/external_texture.h"
#include "flutter/shell/platform/tizen/tizen_renderer.h"
#include "flutter/shell/platform/tizen/tizen_renderer_gl.h"
#include "flutter/shell/platform/tizen/tizen_view_base.h"

namespace flutter {

class TizenRendererEgl : public TizenRendererGL {
 public:
  explicit TizenRendererEgl(TizenViewBase* view_base, bool enable_impeller);

  virtual ~TizenRendererEgl();

  virtual bool OnMakeCurrent() override;

  virtual bool OnClearCurrent() override;

  virtual bool OnMakeResourceCurrent() override;

  virtual bool OnPresent(const FlutterPresentInfo* present_info) override;

  virtual uint32_t OnGetFBO() override;

  virtual void PopulateExistingDamage(
      intptr_t fbo_id,
      FlutterDamage* existing_damage) override;

  virtual void* OnProcResolver(const char* name) override;

  virtual bool IsSupportedExtension(const char* name) override;

  virtual void ResizeSurface(int32_t width, int32_t height) override;

  virtual std::unique_ptr<ExternalTexture> CreateExternalTexture(
      const FlutterDesktopTextureInfo* texture_info) override;

 protected:
  bool CreateSurface(void* render_target,
                     void* render_target_display,
                     int32_t width,
                     int32_t height) override;

  void DestroySurface() override;

 private:
  using EglSwapBuffersWithDamageProc =
      EGLBoolean (*)(EGLDisplay, EGLSurface, const EGLint*, EGLint);
  using EglSetDamageRegionProc =
      EGLBoolean (*)(EGLDisplay, EGLSurface, EGLint*, EGLint);

  bool ChooseEGLConfiguration();

  void ResetDamageTracking();

  bool InitializePartialUpdateSupport();

  bool QuerySurfaceSize(EGLint* width, EGLint* height) const;

  std::vector<EGLint> ConvertDamageToEglRects(
      const FlutterDamage& damage) const;

  void SetFullSurfaceDamage(FlutterDamage* damage);

  void PrintEGLError();

  EGLConfig egl_config_ = nullptr;
  EGLDisplay egl_display_ = EGL_NO_DISPLAY;
  EGLContext egl_context_ = EGL_NO_CONTEXT;
  EGLSurface egl_surface_ = EGL_NO_SURFACE;
  EGLContext egl_resource_context_ = EGL_NO_CONTEXT;
  EGLSurface egl_resource_surface_ = EGL_NO_SURFACE;

  std::string egl_extension_str_;
  bool enable_impeller_;
  bool uses_wayland_display_ = false;
  bool swap_interval_configured_ = false;
  int32_t surface_width_ = 0;
  int32_t surface_height_ = 0;
  bool supports_buffer_age_ = false;
  EglSwapBuffersWithDamageProc egl_swap_buffers_with_damage_ = nullptr;
  EglSetDamageRegionProc egl_set_damage_region_ = nullptr;
  std::deque<FlutterRect> frame_damage_history_;
  std::vector<FlutterRect> existing_damage_storage_;
};

}  // namespace flutter

#endif  // EMBEDDER_TIZEN_RENDERER_EGL_H_
