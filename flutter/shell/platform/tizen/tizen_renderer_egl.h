// Copyright 2020 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_TIZEN_RENDERER_EGL_H_
#define EMBEDDER_TIZEN_RENDERER_EGL_H_

#include <EGL/egl.h>

#include <memory>
#include <string>

#include "flutter/shell/platform/tizen/external_texture.h"
#include "flutter/shell/platform/tizen/tizen_egl_display.h"
#include "flutter/shell/platform/tizen/tizen_renderer.h"
#include "flutter/shell/platform/tizen/tizen_renderer_gl.h"
#include "flutter/shell/platform/tizen/tizen_view_base.h"

namespace flutter {

class TizenRendererEgl : public TizenRendererGL {
 public:
  // Creates a renderer for |view_base|.
  //
  // |share_context| is an existing EGLContext whose resources (textures,
  // shaders, buffers) should be shared with this renderer's onscreen context.
  // Pass EGL_NO_CONTEXT for the first renderer in the process; for subsequent
  // renderers (multi-view), pass the first renderer's onscreen context so that
  // texture and shader caches can be reused across views. The EGLDisplay and
  // EGLConfig are shared through TizenEglDisplay regardless of this argument.
  TizenRendererEgl(TizenViewBase* view_base,
                   bool enable_impeller,
                   EGLContext share_context = EGL_NO_CONTEXT);

  virtual ~TizenRendererEgl();

  virtual bool OnMakeCurrent() override;

  virtual bool OnClearCurrent() override;

  virtual bool OnMakeResourceCurrent() override;

  virtual bool OnPresent() override;

  virtual uint32_t OnGetFBO() override;

  virtual void* OnProcResolver(const char* name) override;

  virtual bool IsSupportedExtension(const char* name) override;

  virtual void ResizeSurface(int32_t width, int32_t height) override;

  virtual std::unique_ptr<ExternalTexture> CreateExternalTexture(
      const FlutterDesktopTextureInfo* texture_info) override;

  // Returns the onscreen EGLContext. Exposed so that additional renderers
  // (for multi-view) can create their own contexts in the same share group,
  // enabling cross-view texture/shader resource sharing.
  EGLContext egl_context() const { return egl_context_; }

 protected:
  bool CreateSurface(void* render_target,
                     void* render_target_display,
                     int32_t width,
                     int32_t height) override;

  void DestroySurface() override;

 private:
  void PrintEGLError();

  // Process-wide shared EGLDisplay wrapper. Retaining the shared_ptr keeps
  // eglInitialize()/eglTerminate() paired exactly once per process, even when
  // multiple TizenRendererEgl instances coexist on different views.
  std::shared_ptr<TizenEglDisplay> egl_display_holder_;

  // Non-owning alias into |egl_display_holder_->egl_display()|. Kept as a
  // member to minimise churn for the many callsites that reference it
  // directly as |egl_display_|.
  EGLDisplay egl_display_ = EGL_NO_DISPLAY;
  EGLConfig egl_config_ = nullptr;

  EGLContext egl_context_ = EGL_NO_CONTEXT;
  EGLSurface egl_surface_ = EGL_NO_SURFACE;
  EGLContext egl_resource_context_ = EGL_NO_CONTEXT;
  EGLSurface egl_resource_surface_ = EGL_NO_SURFACE;

  // Context passed to eglCreateContext() as the share group root. For the
  // first renderer this is EGL_NO_CONTEXT; for later renderers it is the
  // onscreen context of the first renderer.
  EGLContext share_context_ = EGL_NO_CONTEXT;

  std::string egl_extension_str_;
  bool enable_impeller_;
};

}  // namespace flutter

#endif  // EMBEDDER_TIZEN_RENDERER_EGL_H_
