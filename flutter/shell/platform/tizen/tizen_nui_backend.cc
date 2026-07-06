// Copyright 2025 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This translation unit is the only object in the whole embedder that links
// against DALi. It is compiled into a standalone shared library
// (libflutter_tizen_nui.so) that the main embedder loads lazily at runtime.

#include "flutter/shell/platform/tizen/tizen_nui_backend.h"

#include <dali-toolkit/public-api/controls/image-view/image-view.h>
#include <dali/devel-api/adaptor-framework/event-thread-callback.h>
#include <dali/devel-api/adaptor-framework/native-image-source-queue.h>
#include <dali/devel-api/common/stage.h>
#include <dali/public-api/object/any.h>
#include <tbm_surface_queue.h>

#include <memory>

namespace {

void ImageViewGetSize(void* image_view, float* width, float* height) {
  auto* view = static_cast<Dali::Toolkit::ImageView*>(image_view);
  Dali::Vector2 size =
      view->GetProperty(Dali::Actor::Property::SIZE).Get<Dali::Vector2>();
  *width = size.width;
  *height = size.height;
}

void ImageViewSetSize(void* image_view, float width, float height) {
  static_cast<Dali::Toolkit::ImageView*>(image_view)
      ->SetProperty(Dali::Actor::Property::SIZE, Dali::Vector2(width, height));
}

void QueueSetSize(void* queue, int32_t width, int32_t height) {
  static_cast<Dali::NativeImageSourceQueue*>(queue)->SetSize(width, height);
}

void* QueueGetTbmSurfaceQueue(void* queue) {
  auto* native_image_queue = static_cast<Dali::NativeImageSourceQueue*>(queue);
  return Dali::AnyCast<tbm_surface_queue_h>(
      native_image_queue->GetNativeImageSourceQueue());
}

int32_t StageGetDpi() {
  return Dali::Stage::GetCurrent().GetDpi().width;
}

void StageKeepRendering() {
  Dali::Stage::GetCurrent().KeepRendering(0.0f);
}

// Bridges a plain C callback into a Dali::EventThreadCallback, which requires a
// pointer-to-member. Owns the underlying Dali callback for its whole lifetime.
struct NuiEventThreadCallback {
  NuiEventThreadCallback(void (*callback)(void*), void* user_data)
      : callback_(callback), user_data_(user_data) {
    dali_callback_ = std::make_unique<Dali::EventThreadCallback>(
        Dali::MakeCallback(this, &NuiEventThreadCallback::Invoke));
  }

  void Invoke() { callback_(user_data_); }

  void Trigger() { dali_callback_->Trigger(); }

  void (*callback_)(void*);
  void* user_data_;
  std::unique_ptr<Dali::EventThreadCallback> dali_callback_;
};

void* EventThreadCallbackCreate(void (*callback)(void*), void* user_data) {
  return new NuiEventThreadCallback(callback, user_data);
}

void EventThreadCallbackTrigger(void* handle) {
  static_cast<NuiEventThreadCallback*>(handle)->Trigger();
}

void EventThreadCallbackDestroy(void* handle) {
  delete static_cast<NuiEventThreadCallback*>(handle);
}

const FlutterTizenNuiBackend kBackend = {
    ImageViewGetSize,
    ImageViewSetSize,
    QueueSetSize,
    QueueGetTbmSurfaceQueue,
    StageGetDpi,
    StageKeepRendering,
    EventThreadCallbackCreate,
    EventThreadCallbackTrigger,
    EventThreadCallbackDestroy,
};

}  // namespace

extern "C" __attribute__((visibility("default"))) const FlutterTizenNuiBackend*
FlutterTizenNuiBackendGet(void) {
  return &kBackend;
}
