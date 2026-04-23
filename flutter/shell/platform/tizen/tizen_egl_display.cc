// Copyright 2026 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/tizen/tizen_egl_display.h"

#include <wayland-client.h>
#include <tbm_dummy_display.h>

#include <cstdlib>

#include "flutter/shell/platform/tizen/logger.h"

namespace flutter {

namespace {

std::weak_ptr<TizenEglDisplay> g_instance;
std::mutex g_instance_mutex;

void PrintEGLError() {
  EGLint error = eglGetError();
  if (error != EGL_SUCCESS) {
    FT_LOG(Error) << "EGL error: 0x" << std::hex << error;
  }
}

}  // namespace

std::shared_ptr<TizenEglDisplay> TizenEglDisplay::Acquire(
    void* render_target_display,
    bool enable_impeller) {
  std::lock_guard<std::mutex> lock(g_instance_mutex);

  if (auto existing = g_instance.lock()) {
    return existing;
  }

  std::shared_ptr<TizenEglDisplay> display(new TizenEglDisplay());
  if (!display->Init(render_target_display, enable_impeller)) {
    FT_LOG(Error) << "Failed to initialize TizenEglDisplay.";
    return nullptr;
  }

  g_instance = display;
  return display;
}

TizenEglDisplay::~TizenEglDisplay() {
  if (egl_display_ != EGL_NO_DISPLAY) {
    eglTerminate(egl_display_);
    egl_display_ = EGL_NO_DISPLAY;
  }
  egl_config_ = nullptr;
}

bool TizenEglDisplay::Init(void* render_target_display, bool enable_impeller) {
  if (render_target_display) {
    egl_display_ =
        eglGetDisplay(static_cast<wl_display*>(render_target_display));
  } else {
    egl_display_ = eglGetDisplay(tbm_dummy_display_create());
  }

  if (egl_display_ == EGL_NO_DISPLAY) {
    PrintEGLError();
    FT_LOG(Error) << "Could not get EGL display.";
    return false;
  }

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

  if (!ChooseConfig(enable_impeller)) {
    FT_LOG(Error) << "Could not choose an EGL configuration.";
    return false;
  }

  const char* ext = eglQueryString(egl_display_, EGL_EXTENSIONS);
  if (ext) {
    extensions_ = ext;
  }

  return true;
}

bool TizenEglDisplay::ChooseConfig(bool enable_impeller) {
  EGLint config_size = 0;
  if (!eglGetConfigs(egl_display_, nullptr, 0, &config_size)) {
    PrintEGLError();
    FT_LOG(Error) << "Could not query framebuffer configurations.";
    return false;
  }

  EGLConfig* configs =
      static_cast<EGLConfig*>(calloc(config_size, sizeof(EGLConfig)));
  if (!configs) {
    FT_LOG(Error) << "Failed to allocate memory for EGL configurations.";
    return false;
  }

  EGLint num_config = 0;
  if (enable_impeller) {
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
      FT_LOG(Error) << "No matching Impeller configurations found.";
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

  constexpr int kPreferredBufferSize = 32;
  EGLint size = 0;
  for (EGLint i = 0; i < num_config; i++) {
    eglGetConfigAttrib(egl_display_, configs[i], EGL_BUFFER_SIZE, &size);
    if (size == kPreferredBufferSize) {
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

}  // namespace flutter
