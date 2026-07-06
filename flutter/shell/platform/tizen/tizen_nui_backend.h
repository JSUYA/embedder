// Copyright 2025 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_TIZEN_NUI_BACKEND_H_
#define EMBEDDER_TIZEN_NUI_BACKEND_H_

#include <cstdint>

// The NUI/DALi backend interface.
//
// The embedder only depends on DALi in a very limited scenario: hosting a
// Flutter view inside a Tizen NUI application (see
// |FlutterDesktopViewCreateFromImageView|). To avoid a hard link-time
// dependency on the DALi shared libraries for every application - including
// those that never use NUI - all DALi calls are funneled through this plain C
// vtable.
//
// The implementation lives in a separate shared library
// (libflutter_tizen_nui.so) which is the only object that links against DALi.
// The main embedder loads it lazily at runtime (see tizen_nui_backend_loader.h)
// and, when DALi is unavailable, fails gracefully instead of failing to load.
//
// Keeping the boundary a pure C interface (void* handles, POD arguments) avoids
// sharing C++ classes across the shared-library boundary, so there are no
// vtable/typeinfo/symbol-visibility concerns.

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  // Reads the size of a Dali::Toolkit::ImageView* into |width| and |height|.
  void (*image_view_get_size)(void* image_view, float* width, float* height);

  // Sets the size of a Dali::Toolkit::ImageView*.
  void (*image_view_set_size)(void* image_view, float width, float height);

  // Sets the size of a Dali::NativeImageSourceQueue*.
  void (*queue_set_size)(void* queue, int32_t width, int32_t height);

  // Returns the tbm_surface_queue_h backing a Dali::NativeImageSourceQueue*.
  void* (*queue_get_tbm_surface_queue)(void* queue);

  // Returns the horizontal dpi of the current Dali::Stage.
  int32_t (*stage_get_dpi)(void);

  // Requests a single render of the current Dali::Stage.
  void (*stage_keep_rendering)(void);

  // Creates a Dali::EventThreadCallback that invokes |callback| with
  // |user_data| on the DALi event thread. Returns an opaque handle.
  void* (*event_thread_callback_create)(void (*callback)(void* user_data),
                                        void* user_data);

  // Triggers a callback previously created by |event_thread_callback_create|.
  void (*event_thread_callback_trigger)(void* handle);

  // Destroys a callback previously created by |event_thread_callback_create|.
  void (*event_thread_callback_destroy)(void* handle);
} FlutterTizenNuiBackend;

// Entry point exported by libflutter_tizen_nui.so. Returns a pointer to a
// statically-allocated backend vtable (never null). Its mere resolvability
// proves DALi is present, because the library declares DALi as a load-time
// dependency.
const FlutterTizenNuiBackend* FlutterTizenNuiBackendGet(void);

#ifdef __cplusplus
}
#endif

#endif  // EMBEDDER_TIZEN_NUI_BACKEND_H_
