// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_TIZEN_H_
#define FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_TIZEN_H_

#include <memory.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "flutter/shell/platform/embedder/embedder.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum {
  kFlutterGLImpellerTexturePixelBuffer,
  kFlutterGLImpellerTextureGpuSurface,
} FlutterGLImpellerTextureType;

typedef bool (*BoolCallback)(void* /* user data */);

typedef struct {
  /// Callback invoked that texture start binding.
  BoolCallback bind_callback;
  /// The pixel data buffer.
  const uint8_t* buffer;
  /// The size of buffer.
  size_t buffer_size;
  /// The type of the texture.
  FlutterGLImpellerTextureType impeller_texture_type;
  ///
  void* embedder_external_texture;
} FlutterOpenGLTextureTizen;

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif  // FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_TIZEN_H_
