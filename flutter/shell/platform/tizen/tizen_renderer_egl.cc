// Copyright 2020 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/tizen/tizen_renderer_egl.h"

#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#ifdef NUI_SUPPORT
#include <dali/devel-api/adaptor-framework/native-image-source-queue.h>
#endif
#include <tbm_dummy_display.h>
#include <tbm_surface.h>
#include <tbm_surface_queue.h>

#include "flutter/shell/platform/tizen/external_texture_pixel_egl.h"
#include "flutter/shell/platform/tizen/external_texture_surface_egl.h"
#include "flutter/shell/platform/tizen/logger.h"

#ifndef EGL_BUFFER_AGE_EXT
#define EGL_BUFFER_AGE_EXT 0x313D
#endif

namespace flutter {

// [TEMP_DIAG_REMOVE] Verbose runtime diagnostics for blank-screen triage.
#define TEMP_DIAG_EGL(msg) do { } while (0)  // [TEMP_DIAG_REMOVE]

namespace {

constexpr size_t kMaxDamageHistory = 10;

bool HasExtensionToken(const std::string& extensions, const char* name) {
  const size_t name_length = strlen(name);
  size_t offset = extensions.find(name);
  while (offset != std::string::npos) {
    const bool has_prefix =
        offset == 0 || extensions[offset - 1] == ' ';
    const size_t suffix_offset = offset + name_length;
    const bool has_suffix =
        suffix_offset == extensions.size() || extensions[suffix_offset] == ' ';
    if (has_prefix && has_suffix) {
      return true;
    }
    offset = extensions.find(name, offset + name_length);
  }
  return false;
}

bool IsEmptyRect(const FlutterRect& rect) {
  return rect.right <= rect.left || rect.bottom <= rect.top;
}

FlutterRect MakeEmptyRect() {
  return FlutterRect{0, 0, 0, 0};
}

FlutterRect UnionRect(const FlutterRect& lhs, const FlutterRect& rhs) {
  if (IsEmptyRect(lhs)) {
    return rhs;
  }
  if (IsEmptyRect(rhs)) {
    return lhs;
  }
  return FlutterRect{
      std::min(lhs.left, rhs.left), std::min(lhs.top, rhs.top),
      std::max(lhs.right, rhs.right), std::max(lhs.bottom, rhs.bottom)};
}

FlutterRect MergeDamageRectangles(const FlutterDamage& damage) {
  FlutterRect merged = MakeEmptyRect();
  if (damage.damage == nullptr) {
    return merged;
  }
  for (size_t i = 0; i < damage.num_rects; ++i) {
    merged = UnionRect(merged, damage.damage[i]);
  }
  return merged;
}

}  // namespace

TizenRendererEgl::TizenRendererEgl(TizenViewBase* view_base,
                                   bool enable_impeller)
    : enable_impeller_(enable_impeller) {
  TizenRenderer::CreateSurface(view_base);
}

TizenRendererEgl::~TizenRendererEgl() {
  DestroySurface();
}

std::unique_ptr<ExternalTexture> TizenRendererEgl::CreateExternalTexture(
    const FlutterDesktopTextureInfo* texture_info) {
  switch (texture_info->type) {
    case kFlutterDesktopPixelBufferTexture:
      return std::make_unique<ExternalTexturePixelEGL>(
          texture_info->pixel_buffer_config.callback,
          texture_info->pixel_buffer_config.user_data);
    case kFlutterDesktopGpuSurfaceTexture:
      return std::make_unique<ExternalTextureSurfaceEGL>(
          GetExternalTextureExtensionType(),
          texture_info->gpu_surface_config.callback,
          texture_info->gpu_surface_config.user_data);
  }
}

bool TizenRendererEgl::CreateSurface(void* render_target,
                                     void* render_target_display,
                                     int32_t width,
                                     int32_t height) {
  TEMP_DIAG_EGL("CreateSurface begin. render_target=" << render_target
               << " display=" << render_target_display << " size=" << width
               << "x" << height);
  surface_width_ = width;
  surface_height_ = height;
  ResetDamageTracking();
  swap_interval_configured_ = false;
  if (render_target_display) {
    uses_wayland_display_ = true;
    auto* wayland_display = static_cast<struct wl_display*>(render_target_display);

    egl_display_ = eglGetDisplay(wayland_display);

    if (egl_display_ == EGL_NO_DISPLAY) {
      auto* get_platform_display =
          reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
              eglGetProcAddress("eglGetPlatformDisplayEXT"));
      if (get_platform_display) {
        egl_display_ = get_platform_display(EGL_PLATFORM_WAYLAND_KHR,
                                            wayland_display, nullptr);
      }
    }
  } else {
    uses_wayland_display_ = false;
    egl_display_ = eglGetDisplay(tbm_dummy_display_create());
  }

  if (egl_display_ == EGL_NO_DISPLAY) {
    PrintEGLError();
    FT_LOG(Error) << "Could not get EGL display.";
    return false;
  }
  TEMP_DIAG_EGL("EGL display acquired. egl_display=" << egl_display_);

  if (!ChooseEGLConfiguration()) {
    FT_LOG(Error) << "Could not choose an EGL configuration.";
    return false;
  }

  const char* extensions = eglQueryString(egl_display_, EGL_EXTENSIONS);
  egl_extension_str_ = extensions ? extensions : "";

  {
    const EGLint attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

    egl_context_ =
        eglCreateContext(egl_display_, egl_config_, EGL_NO_CONTEXT, attribs);
    if (egl_context_ == EGL_NO_CONTEXT) {
      PrintEGLError();
      FT_LOG(Error) << "Could not create an onscreen context.";
      return false;
    }

    egl_resource_context_ =
        eglCreateContext(egl_display_, egl_config_, egl_context_, attribs);
    if (egl_resource_context_ == EGL_NO_CONTEXT) {
      PrintEGLError();
      FT_LOG(Error) << "Could not create an offscreen context.";
      return false;
    }
  }

  {
    const EGLint attribs[] = {EGL_NONE};

    if (render_target_display) {
      auto egl_window = static_cast<EGLNativeWindowType>(render_target);
      egl_surface_ = eglCreateWindowSurface(egl_display_, egl_config_,
                                            egl_window, attribs);
    } else {
#ifdef NUI_SUPPORT
      Dali::NativeImageSourceQueuePtr dali_native_image_queue =
          static_cast<Dali::NativeImageSourceQueue*>(render_target);
      tbm_surface_queue_h tbm_surface_queue_ =
          Dali::AnyCast<tbm_surface_queue_h>(
              dali_native_image_queue->GetNativeImageSourceQueue());
      auto* egl_window =
          reinterpret_cast<EGLNativeWindowType*>(tbm_surface_queue_);
      egl_surface_ = eglCreateWindowSurface(egl_display_, egl_config_,
                                            egl_window, attribs);
#endif
    }

    if (egl_surface_ == EGL_NO_SURFACE) {
      FT_LOG(Error) << "Could not create an onscreen window surface.";
      return false;
    }

    FT_LOG(Info) << "EGL onscreen surface created. render_target="
                 << render_target << " display=" << render_target_display;
  }

  {
    const EGLint attribs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};

    egl_resource_surface_ =
        eglCreatePbufferSurface(egl_display_, egl_config_, attribs);
    if (egl_resource_surface_ == EGL_NO_SURFACE) {
      FT_LOG(Error) << "Could not create an offscreen window surface.";
      return false;
    }
  }

  InitializePartialUpdateSupport();
  is_valid_ = true;
  return true;
}

void TizenRendererEgl::DestroySurface() {
  if (egl_display_) {
    eglMakeCurrent(egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);

    if (EGL_NO_SURFACE != egl_surface_) {
      eglDestroySurface(egl_display_, egl_surface_);
      egl_surface_ = EGL_NO_SURFACE;
    }

    if (EGL_NO_CONTEXT != egl_context_) {
      eglDestroyContext(egl_display_, egl_context_);
      egl_context_ = EGL_NO_CONTEXT;
    }

    if (EGL_NO_SURFACE != egl_resource_surface_) {
      eglDestroySurface(egl_display_, egl_resource_surface_);
      egl_resource_surface_ = EGL_NO_SURFACE;
    }

    if (EGL_NO_CONTEXT != egl_resource_context_) {
      eglDestroyContext(egl_display_, egl_resource_context_);
      egl_resource_context_ = EGL_NO_CONTEXT;
    }

    eglTerminate(egl_display_);
    egl_display_ = EGL_NO_DISPLAY;
  }
  ResetDamageTracking();
  surface_width_ = 0;
  surface_height_ = 0;
  uses_wayland_display_ = false;
  swap_interval_configured_ = false;
}

bool TizenRendererEgl::ChooseEGLConfiguration() {
  if (!eglInitialize(egl_display_, nullptr, nullptr)) {
    PrintEGLError();
    FT_LOG(Error) << "Could not initialize the EGL display.";
    return false;
  }

  if (!eglBindAPI(EGL_OPENGL_ES_API)) {
    PrintEGLError();
    FT_LOG(Error) << "Could not bind the ES API.";
    return false;
  }

  EGLint config_size = 0;
  if (!eglGetConfigs(egl_display_, nullptr, 0, &config_size)) {
    PrintEGLError();
    FT_LOG(Error) << "Could not query framebuffer configurations.";
    return false;
  }

  EGLConfig* configs = (EGLConfig*)calloc(config_size, sizeof(EGLConfig));
  if (!configs) {
    FT_LOG(Error) << "Failed to allocate memory for EGL configurations.";
    return false;
  }
  EGLint num_config;
  if (enable_impeller_) {
    EGLint impeller_config_attribs[] = {
        // clang-format off
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RED_SIZE,        8,
        EGL_GREEN_SIZE,      8,
        EGL_BLUE_SIZE,       8,
        EGL_ALPHA_SIZE,      8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SAMPLE_BUFFERS,  1,
        EGL_SAMPLES,         4,
        EGL_STENCIL_SIZE,    8,
        EGL_DEPTH_SIZE,      0,
        EGL_NONE
        // clang-format on
    };
    if (!eglChooseConfig(egl_display_, impeller_config_attribs, configs,
                         config_size, &num_config)) {
      free(configs);
      PrintEGLError();
      FT_LOG(Error) << "No matching configurations found.";
      return false;
    }
  } else {
    EGLint config_attribs[] = {
        // clang-format off
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RED_SIZE,        8,
        EGL_GREEN_SIZE,      8,
        EGL_BLUE_SIZE,       8,
        EGL_ALPHA_SIZE,      EGL_DONT_CARE,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SAMPLE_BUFFERS,  EGL_DONT_CARE,
        EGL_SAMPLES,         EGL_DONT_CARE,
        EGL_NONE
        // clang-format on
    };
    if (!eglChooseConfig(egl_display_, config_attribs, configs, config_size,
                         &num_config)) {
      free(configs);
      PrintEGLError();
      FT_LOG(Error) << "No matching configurations found.";
      return false;
    }
  }

  int buffer_size = 32;
  EGLint size;
  for (int i = 0; i < num_config; i++) {
    eglGetConfigAttrib(egl_display_, configs[i], EGL_BUFFER_SIZE, &size);
    if (buffer_size == size) {
      egl_config_ = configs[i];
      break;
    }
  }
  free(configs);
  if (!egl_config_) {
    FT_LOG(Error) << "No matching configuration found.";
    return false;
  }

  return true;
}

void TizenRendererEgl::ResetDamageTracking() {
  supports_buffer_age_ = false;
  egl_swap_buffers_with_damage_ = nullptr;
  egl_set_damage_region_ = nullptr;
  frame_damage_history_.clear();
  existing_damage_storage_.clear();
}

bool TizenRendererEgl::InitializePartialUpdateSupport() {
  if (!uses_wayland_display_) {
    return false;
  }

  supports_buffer_age_ =
      HasExtensionToken(egl_extension_str_, "EGL_EXT_buffer_age") ||
      HasExtensionToken(egl_extension_str_, "EGL_KHR_partial_update");

  if (HasExtensionToken(egl_extension_str_, "EGL_KHR_partial_update")) {
    egl_set_damage_region_ = reinterpret_cast<EglSetDamageRegionProc>(
        eglGetProcAddress("eglSetDamageRegionKHR"));
  }

  if (HasExtensionToken(egl_extension_str_, "EGL_EXT_swap_buffers_with_damage")) {
    egl_swap_buffers_with_damage_ =
        reinterpret_cast<EglSwapBuffersWithDamageProc>(
            eglGetProcAddress("eglSwapBuffersWithDamageEXT"));
  }
  if (!egl_swap_buffers_with_damage_ &&
      HasExtensionToken(egl_extension_str_, "EGL_KHR_swap_buffers_with_damage")) {
    egl_swap_buffers_with_damage_ =
        reinterpret_cast<EglSwapBuffersWithDamageProc>(
            eglGetProcAddress("eglSwapBuffersWithDamageKHR"));
  }
  if (!egl_swap_buffers_with_damage_) {
    egl_swap_buffers_with_damage_ =
        reinterpret_cast<EglSwapBuffersWithDamageProc>(
            eglGetProcAddress("eglSwapBuffersWithDamage"));
  }

  return supports_buffer_age_ || egl_set_damage_region_ ||
         egl_swap_buffers_with_damage_;
}

bool TizenRendererEgl::QuerySurfaceSize(EGLint* width, EGLint* height) const {
  EGLint queried_width = surface_width_;
  EGLint queried_height = surface_height_;
  if (egl_display_ != EGL_NO_DISPLAY && egl_surface_ != EGL_NO_SURFACE) {
    if (!eglQuerySurface(egl_display_, egl_surface_, EGL_WIDTH,
                         &queried_width)) {
      queried_width = surface_width_;
    }
    if (!eglQuerySurface(egl_display_, egl_surface_, EGL_HEIGHT,
                         &queried_height)) {
      queried_height = surface_height_;
    }
  }
  if (queried_width <= 0 || queried_height <= 0) {
    return false;
  }
  if (width) {
    *width = queried_width;
  }
  if (height) {
    *height = queried_height;
  }
  return true;
}

std::vector<EGLint> TizenRendererEgl::ConvertDamageToEglRects(
    const FlutterDamage& damage) const {
  std::vector<EGLint> rects;
  if (damage.damage == nullptr) {
    return rects;
  }
  EGLint surface_width = 0;
  EGLint surface_height = 0;
  if (!QuerySurfaceSize(&surface_width, &surface_height)) {
    return rects;
  }

  rects.reserve(damage.num_rects * 4);
  for (size_t i = 0; i < damage.num_rects; ++i) {
    const auto& rect = damage.damage[i];
    const EGLint left = std::min(
        std::max(static_cast<EGLint>(std::floor(rect.left)), 0), surface_width);
    const EGLint top = std::min(
        std::max(static_cast<EGLint>(std::floor(rect.top)), 0),
        surface_height);
    const EGLint right = std::min(
        std::max(static_cast<EGLint>(std::ceil(rect.right)), 0), surface_width);
    const EGLint bottom = std::min(
        std::max(static_cast<EGLint>(std::ceil(rect.bottom)), 0),
        surface_height);
    if (right <= left || bottom <= top) {
      continue;
    }
    rects.push_back(left);
    rects.push_back(surface_height - bottom);
    rects.push_back(right - left);
    rects.push_back(bottom - top);
  }
  return rects;
}

void TizenRendererEgl::SetFullSurfaceDamage(FlutterDamage* damage) {
  EGLint width = 0;
  EGLint height = 0;
  if (!QuerySurfaceSize(&width, &height)) {
    damage->num_rects = 0;
    damage->damage = nullptr;
    return;
  }
  existing_damage_storage_.assign(
      1, FlutterRect{0, 0, static_cast<double>(width),
                     static_cast<double>(height)});
  damage->num_rects = existing_damage_storage_.size();
  damage->damage = existing_damage_storage_.data();
}

bool TizenRendererEgl::OnMakeCurrent() {
  if (!IsValid()) {
    return false;
  }
  if (eglMakeCurrent(egl_display_, egl_surface_, egl_surface_, egl_context_) !=
      EGL_TRUE) {
    PrintEGLError();
    FT_LOG(Error) << "Could not make the onscreen context current.";
    return false;
  }

  if (uses_wayland_display_ && !swap_interval_configured_) {
    // EFL's public Wayland EGL path disables EGL's own swap pacing and lets
    // the Wayland/compositor path own presentation timing.
    swap_interval_configured_ = true;
    if (eglSwapInterval(egl_display_, 0) != EGL_TRUE) {
      PrintEGLError();
      FT_LOG(Warn) << "Could not disable EGL swap interval for Wayland.";
    }
  }
  return true;
}

bool TizenRendererEgl::OnClearCurrent() {
  if (!IsValid()) {
    return false;
  }
  if (eglMakeCurrent(egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE,
                     EGL_NO_CONTEXT) != EGL_TRUE) {
    PrintEGLError();
    FT_LOG(Error) << "Could not clear the context.";
    return false;
  }
  return true;
}

bool TizenRendererEgl::OnMakeResourceCurrent() {
  if (!IsValid()) {
    return false;
  }
  if (eglMakeCurrent(egl_display_, egl_resource_surface_, egl_resource_surface_,
                     egl_resource_context_) != EGL_TRUE) {
    PrintEGLError();
    FT_LOG(Error) << "Could not make the offscreen context current.";
    return false;
  }
  return true;
}

bool TizenRendererEgl::OnPresent(const FlutterPresentInfo* present_info) {
  if (!IsValid()) {
    return false;
  }

  if (present_info && egl_set_damage_region_) {
    auto buffer_rects = ConvertDamageToEglRects(present_info->buffer_damage);
    if (egl_set_damage_region_(egl_display_, egl_surface_,
                               buffer_rects.empty() ? nullptr
                                                    : buffer_rects.data(),
                               buffer_rects.size() / 4) != EGL_TRUE) {
      PrintEGLError();
      FT_LOG(Warn) << "Could not set EGL damage region.";
      egl_set_damage_region_ = nullptr;
    }
  }

  EGLBoolean swap_result = EGL_FALSE;
  if (present_info && egl_swap_buffers_with_damage_) {
    auto frame_rects = ConvertDamageToEglRects(present_info->frame_damage);
    swap_result = egl_swap_buffers_with_damage_(
        egl_display_, egl_surface_,
        frame_rects.empty() ? nullptr : frame_rects.data(),
        frame_rects.size() / 4);
    if (swap_result != EGL_TRUE) {
      PrintEGLError();
      FT_LOG(Warn) << "Could not swap EGL buffers with damage.";
      egl_swap_buffers_with_damage_ = nullptr;
      swap_result = eglSwapBuffers(egl_display_, egl_surface_);
    }
  } else {
    swap_result = eglSwapBuffers(egl_display_, egl_surface_);
  }

  if (swap_result != EGL_TRUE) {
    PrintEGLError();
    FT_LOG(Error) << "Could not swap EGL buffers.";
    return false;
  }

  if (present_info && uses_wayland_display_) {
    frame_damage_history_.push_back(MergeDamageRectangles(
        present_info->frame_damage));
    if (frame_damage_history_.size() > kMaxDamageHistory) {
      frame_damage_history_.pop_front();
    }
  }

  // [TEMP_DIAG_REMOVE] Avoid per-frame logging; it severely impacts pointer
  // responsiveness on low-power targets.
  return true;
}

uint32_t TizenRendererEgl::OnGetFBO() {
  if (!IsValid()) {
    return 999;
  }
  return 0;
}

void TizenRendererEgl::PopulateExistingDamage(
    intptr_t fbo_id,
    FlutterDamage* existing_damage) {
  existing_damage->num_rects = 0;
  existing_damage->damage = nullptr;

  if (!uses_wayland_display_ || fbo_id != 0 || !supports_buffer_age_) {
    SetFullSurfaceDamage(existing_damage);
    return;
  }

  EGLint age = 0;
  if (eglQuerySurface(egl_display_, egl_surface_, EGL_BUFFER_AGE_EXT, &age) !=
          EGL_TRUE ||
      age <= 0) {
    frame_damage_history_.clear();
    existing_damage_storage_.clear();
    SetFullSurfaceDamage(existing_damage);
    return;
  }

  if (age == 1) {
    return;
  }

  const size_t required_history = static_cast<size_t>(age - 1);
  if (required_history > frame_damage_history_.size() ||
      required_history > kMaxDamageHistory) {
    SetFullSurfaceDamage(existing_damage);
    return;
  }

  FlutterRect merged = MakeEmptyRect();
  for (size_t i = 0; i < required_history; ++i) {
    merged = UnionRect(merged, frame_damage_history_[frame_damage_history_.size() -
                                                     1 - i]);
  }
  if (IsEmptyRect(merged)) {
    return;
  }

  existing_damage_storage_.assign(1, merged);
  existing_damage->num_rects = existing_damage_storage_.size();
  existing_damage->damage = existing_damage_storage_.data();
}

void TizenRendererEgl::PrintEGLError() {
  EGLint error = eglGetError();
  switch (error) {
#define CASE_PRINT(value)                     \
  case value: {                               \
    FT_LOG(Error) << "EGL error: " << #value; \
    break;                                    \
  }
    CASE_PRINT(EGL_NOT_INITIALIZED)
    CASE_PRINT(EGL_BAD_ACCESS)
    CASE_PRINT(EGL_BAD_ALLOC)
    CASE_PRINT(EGL_BAD_ATTRIBUTE)
    CASE_PRINT(EGL_BAD_CONTEXT)
    CASE_PRINT(EGL_BAD_CONFIG)
    CASE_PRINT(EGL_BAD_CURRENT_SURFACE)
    CASE_PRINT(EGL_BAD_DISPLAY)
    CASE_PRINT(EGL_BAD_SURFACE)
    CASE_PRINT(EGL_BAD_MATCH)
    CASE_PRINT(EGL_BAD_PARAMETER)
    CASE_PRINT(EGL_BAD_NATIVE_PIXMAP)
    CASE_PRINT(EGL_BAD_NATIVE_WINDOW)
    CASE_PRINT(EGL_CONTEXT_LOST)
#undef CASE_PRINT
    default: {
      FT_LOG(Error) << "Unknown EGL error: " << error;
    }
  }
}

bool TizenRendererEgl::IsSupportedExtension(const char* name) {
  return strstr(egl_extension_str_.c_str(), name);
}

void TizenRendererEgl::ResizeSurface(int32_t width, int32_t height) {
  surface_width_ = width;
  surface_height_ = height;
  frame_damage_history_.clear();
  existing_damage_storage_.clear();
}

void* TizenRendererEgl::OnProcResolver(const char* name) {
  auto address = eglGetProcAddress(name);
  if (address != nullptr) {
    return reinterpret_cast<void*>(address);
  }
#define GL_FUNC(FunctionName)                     \
  else if (strcmp(name, #FunctionName) == 0) {    \
    return reinterpret_cast<void*>(FunctionName); \
  }
  GL_FUNC(eglGetCurrentDisplay)
  GL_FUNC(eglQueryString)
  GL_FUNC(glActiveTexture)
  GL_FUNC(glAttachShader)
  GL_FUNC(glBindAttribLocation)
  GL_FUNC(glBindBuffer)
  GL_FUNC(glBindFramebuffer)
  GL_FUNC(glBindRenderbuffer)
  GL_FUNC(glBindTexture)
  GL_FUNC(glBlendColor)
  GL_FUNC(glBlendEquation)
  GL_FUNC(glBlendFunc)
  GL_FUNC(glBufferData)
  GL_FUNC(glBufferSubData)
  GL_FUNC(glCheckFramebufferStatus)
  GL_FUNC(glClear)
  GL_FUNC(glClearColor)
  GL_FUNC(glClearStencil)
  GL_FUNC(glColorMask)
  GL_FUNC(glCompileShader)
  GL_FUNC(glCompressedTexImage2D)
  GL_FUNC(glCompressedTexSubImage2D)
  GL_FUNC(glCopyTexSubImage2D)
  GL_FUNC(glCreateProgram)
  GL_FUNC(glCreateShader)
  GL_FUNC(glCullFace)
  GL_FUNC(glDeleteBuffers)
  GL_FUNC(glDeleteFramebuffers)
  GL_FUNC(glDeleteProgram)
  GL_FUNC(glDeleteRenderbuffers)
  GL_FUNC(glDeleteShader)
  GL_FUNC(glDeleteTextures)
  GL_FUNC(glDepthMask)
  GL_FUNC(glDisable)
  GL_FUNC(glDisableVertexAttribArray)
  GL_FUNC(glDrawArrays)
  GL_FUNC(glDrawElements)
  GL_FUNC(glEnable)
  GL_FUNC(glEnableVertexAttribArray)
  GL_FUNC(glFinish)
  GL_FUNC(glFlush)
  GL_FUNC(glFramebufferRenderbuffer)
  GL_FUNC(glFramebufferTexture2D)
  GL_FUNC(glFrontFace)
  GL_FUNC(glGenBuffers)
  GL_FUNC(glGenerateMipmap)
  GL_FUNC(glGenFramebuffers)
  GL_FUNC(glGenRenderbuffers)
  GL_FUNC(glGenTextures)
  GL_FUNC(glGetBufferParameteriv)
  GL_FUNC(glGetError)
  GL_FUNC(glGetFloatv)
  GL_FUNC(glGetFramebufferAttachmentParameteriv)
  GL_FUNC(glGetIntegerv)
  GL_FUNC(glGetProgramInfoLog)
  GL_FUNC(glGetProgramiv)
  GL_FUNC(glGetRenderbufferParameteriv)
  GL_FUNC(glGetShaderInfoLog)
  GL_FUNC(glGetShaderiv)
  GL_FUNC(glGetShaderPrecisionFormat)
  GL_FUNC(glGetString)
  GL_FUNC(glGetUniformLocation)
  GL_FUNC(glIsTexture)
  GL_FUNC(glLineWidth)
  GL_FUNC(glLinkProgram)
  GL_FUNC(glPixelStorei)
  GL_FUNC(glReadPixels)
  GL_FUNC(glRenderbufferStorage)
  GL_FUNC(glScissor)
  GL_FUNC(glShaderSource)
  GL_FUNC(glStencilFunc)
  GL_FUNC(glStencilFuncSeparate)
  GL_FUNC(glStencilMask)
  GL_FUNC(glStencilMaskSeparate)
  GL_FUNC(glStencilOp)
  GL_FUNC(glStencilOpSeparate)
  GL_FUNC(glTexImage2D)
  GL_FUNC(glTexParameterf)
  GL_FUNC(glTexParameterfv)
  GL_FUNC(glTexParameteri)
  GL_FUNC(glTexParameteriv)
  GL_FUNC(glTexSubImage2D)
  GL_FUNC(glUniform1f)
  GL_FUNC(glUniform1fv)
  GL_FUNC(glUniform1i)
  GL_FUNC(glUniform1iv)
  GL_FUNC(glUniform2f)
  GL_FUNC(glUniform2fv)
  GL_FUNC(glUniform2i)
  GL_FUNC(glUniform2iv)
  GL_FUNC(glUniform3f)
  GL_FUNC(glUniform3fv)
  GL_FUNC(glUniform3i)
  GL_FUNC(glUniform3iv)
  GL_FUNC(glUniform4f)
  GL_FUNC(glUniform4fv)
  GL_FUNC(glUniform4i)
  GL_FUNC(glUniform4iv)
  GL_FUNC(glUniformMatrix2fv)
  GL_FUNC(glUniformMatrix3fv)
  GL_FUNC(glUniformMatrix4fv)
  GL_FUNC(glUseProgram)
  GL_FUNC(glVertexAttrib1f)
  GL_FUNC(glVertexAttrib2fv)
  GL_FUNC(glVertexAttrib3fv)
  GL_FUNC(glVertexAttrib4fv)
  GL_FUNC(glVertexAttribPointer)
  GL_FUNC(glViewport)
#define GL_FUNC_EXT(ExtFunctionName, FunctionName)                    \
  else if (strcmp(name, #ExtFunctionName) == 0) {                     \
    return reinterpret_cast<void*>(eglGetProcAddress(#FunctionName)); \
  }
  GL_FUNC_EXT(glMultiDrawArraysIndirectEXT, glMultiDrawArraysIndirect)
  GL_FUNC_EXT(glMultiDrawElementsIndirectEXT, glMultiDrawElementsIndirect)
#undef GL_FUNC_EXT
#undef GL_FUNC

  FT_LOG(Warn) << "Could not resolve: " << name;
  return nullptr;
}
}  // namespace flutter
