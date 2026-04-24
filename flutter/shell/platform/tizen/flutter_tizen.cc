// Copyright 2020 Samsung Electronics Co., Ltd. All rights reserved.
// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/flutter_tizen.h"

#include "flutter/shell/platform/common/client_wrapper/include/flutter/plugin_registrar.h"
#include "flutter/shell/platform/common/client_wrapper/include/flutter/standard_message_codec.h"
#include "flutter/shell/platform/common/incoming_message_dispatcher.h"
#include "flutter/shell/platform/embedder/embedder.h"
#include "flutter/shell/platform/tizen/flutter_project_bundle.h"
#include "flutter/shell/platform/tizen/flutter_tizen_engine.h"
#include "flutter/shell/platform/tizen/flutter_tizen_view.h"
#include "flutter/shell/platform/tizen/logger.h"
#include "flutter/shell/platform/tizen/public/flutter_platform_view.h"
#include "flutter/shell/platform/tizen/tizen_view.h"
#include "flutter/shell/platform/tizen/tizen_view_base.h"
#ifdef NUI_SUPPORT
#include "flutter/shell/platform/tizen/tizen_renderer_egl.h"
#include "flutter/shell/platform/tizen/tizen_view_nui.h"
#endif
#include "flutter/shell/platform/tizen/tizen_window.h"
#include "flutter/shell/platform/tizen/tizen_window_ecore_wl2.h"

namespace {

// Returns the engine corresponding to the given opaque API handle.
flutter::FlutterTizenEngine* EngineFromHandle(FlutterDesktopEngineRef ref) {
  return reinterpret_cast<flutter::FlutterTizenEngine*>(ref);
}

// Returns the opaque API handle for the given engine instance.
FlutterDesktopEngineRef HandleForEngine(flutter::FlutterTizenEngine* engine) {
  return reinterpret_cast<FlutterDesktopEngineRef>(engine);
}

// Returns the view corresponding to the given opaque API handle.
flutter::FlutterTizenView* ViewFromHandle(FlutterDesktopViewRef view) {
  return reinterpret_cast<flutter::FlutterTizenView*>(view);
}

// Returns the texture registrar corresponding to the given opaque API handle.
flutter::FlutterTizenTextureRegistrar* TextureRegistrarFromHandle(
    FlutterDesktopTextureRegistrarRef ref) {
  return reinterpret_cast<flutter::FlutterTizenTextureRegistrar*>(ref);
}

// Returns the opaque API handle for the given texture registrar instance.
FlutterDesktopTextureRegistrarRef HandleForTextureRegistrar(
    flutter::FlutterTizenTextureRegistrar* registrar) {
  return reinterpret_cast<FlutterDesktopTextureRegistrarRef>(registrar);
}

FlutterDesktopViewRef HandleForView(flutter::FlutterTizenView* view) {
  return reinterpret_cast<FlutterDesktopViewRef>(view);
}

}  // namespace

FlutterDesktopEngineRef FlutterDesktopEngineCreate(
    const FlutterDesktopEngineProperties& engine_properties) {
  flutter::FlutterProjectBundle project(engine_properties);
  if (project.HasArgument("--verbose-logging")) {
    flutter::Logger::SetLoggingLevel(flutter::kLogLevelDebug);
  }
  std::string logging_port;
  if (project.GetArgumentValue("--tizen-logging-port", &logging_port)) {
    flutter::Logger::SetLoggingPort(std::stoi(logging_port));
  }
  flutter::Logger::Start();

  auto engine = std::make_unique<flutter::FlutterTizenEngine>(project);
  return HandleForEngine(engine.release());
}

bool FlutterDesktopEngineRun(const FlutterDesktopEngineRef engine) {
  return EngineFromHandle(engine)->RunEngine();
}

void FlutterDesktopEngineShutdown(FlutterDesktopEngineRef engine_ref) {
  flutter::Logger::Stop();

  flutter::FlutterTizenEngine* engine = EngineFromHandle(engine_ref);
  engine->StopEngine();
  delete engine;
}

FlutterDesktopViewRef FlutterDesktopPluginRegistrarGetView(
    FlutterDesktopPluginRegistrarRef registrar) {
  return HandleForView(registrar->engine->view());
}

void FlutterDesktopPluginRegistrarEnableInputBlocking(
    FlutterDesktopPluginRegistrarRef registrar,
    const char* channel) {
  registrar->engine->message_dispatcher()->EnableInputBlockingForChannel(
      channel);
}

FlutterDesktopPluginRegistrarRef FlutterDesktopEngineGetPluginRegistrar(
    FlutterDesktopEngineRef engine,
    const char* plugin_name) {
  // Currently, one registrar acts as the registrar for all plugins, so the
  // name is ignored. It is part of the API to reduce churn in the future when
  // aligning more closely with the Flutter registrar system.
  return EngineFromHandle(engine)->plugin_registrar();
}

FlutterDesktopMessengerRef FlutterDesktopEngineGetMessenger(
    FlutterDesktopEngineRef engine) {
  return EngineFromHandle(engine)->messenger();
}

FlutterDesktopMessengerRef FlutterDesktopPluginRegistrarGetMessenger(
    FlutterDesktopPluginRegistrarRef registrar) {
  return registrar->engine->messenger();
}

void FlutterDesktopPluginRegistrarSetDestructionHandler(
    FlutterDesktopPluginRegistrarRef registrar,
    FlutterDesktopOnPluginRegistrarDestroyed callback) {
  registrar->engine->AddPluginRegistrarDestructionCallback(callback, registrar);
}

bool FlutterDesktopMessengerSend(FlutterDesktopMessengerRef messenger,
                                 const char* channel,
                                 const uint8_t* message,
                                 const size_t message_size) {
  return FlutterDesktopMessengerSendWithReply(messenger, channel, message,
                                              message_size, nullptr, nullptr);
}

bool FlutterDesktopMessengerSendWithReply(FlutterDesktopMessengerRef messenger,
                                          const char* channel,
                                          const uint8_t* message,
                                          const size_t message_size,
                                          const FlutterDesktopBinaryReply reply,
                                          void* user_data) {
  return messenger->engine->SendPlatformMessage(channel, message, message_size,
                                                reply, user_data);
}

void FlutterDesktopMessengerSendResponse(
    FlutterDesktopMessengerRef messenger,
    const FlutterDesktopMessageResponseHandle* handle,
    const uint8_t* data,
    size_t data_length) {
  messenger->engine->SendPlatformMessageResponse(handle, data, data_length);
}

void FlutterDesktopMessengerSetCallback(FlutterDesktopMessengerRef messenger,
                                        const char* channel,
                                        FlutterDesktopMessageCallback callback,
                                        void* user_data) {
  messenger->engine->message_dispatcher()->SetMessageCallback(channel, callback,
                                                              user_data);
}

void FlutterDesktopEngineNotifyAppControl(FlutterDesktopEngineRef engine,
                                          void* app_control) {
  EngineFromHandle(engine)->app_control_channel()->NotifyAppControl(
      app_control);
}

void FlutterDesktopEngineNotifyLocaleChange(FlutterDesktopEngineRef engine) {
  EngineFromHandle(engine)->SetupLocales();
}

void FlutterDesktopEngineNotifyLowMemoryWarning(
    FlutterDesktopEngineRef engine) {
  EngineFromHandle(engine)->NotifyLowMemoryWarning();
}

void FlutterDesktopEngineNotifyAppIsInactive(FlutterDesktopEngineRef engine) {
  EngineFromHandle(engine)->lifecycle_channel()->AppIsInactive();
}

void FlutterDesktopEngineNotifyAppIsResumed(FlutterDesktopEngineRef engine) {
  EngineFromHandle(engine)->lifecycle_channel()->AppIsResumed();
}

void FlutterDesktopEngineNotifyAppIsPaused(FlutterDesktopEngineRef engine) {
  EngineFromHandle(engine)->lifecycle_channel()->AppIsPaused();
}

void FlutterDesktopEngineNotifyAppIsDetached(FlutterDesktopEngineRef engine) {
  EngineFromHandle(engine)->lifecycle_channel()->AppIsDetached();
}

void FlutterDesktopViewDestroy(FlutterDesktopViewRef view_ref) {
  flutter::FlutterTizenView* view = ViewFromHandle(view_ref);
  delete view;
}

FlutterDesktopViewRef FlutterDesktopViewCreateFromNewWindow(
    const FlutterDesktopWindowProperties& window_properties,
    FlutterDesktopEngineRef engine) {
  flutter::TizenGeometry window_geometry = {
      window_properties.x, window_properties.y, window_properties.width,
      window_properties.height};

  std::unique_ptr<flutter::TizenWindow> window;

  window = std::make_unique<flutter::TizenWindowEcoreWl2>(
      window_geometry, window_properties.transparent,
      window_properties.focusable, window_properties.top_level,
      window_properties.pointing_device_support,
      window_properties.floating_menu_support, window_properties.window_handle,
      window_properties.renderer_type == kEVulkan);

  auto view = std::make_unique<flutter::FlutterTizenView>(
      flutter::kImplicitViewId, std::move(window),
      std::unique_ptr<flutter::FlutterTizenEngine>(EngineFromHandle(engine)),
      window_properties.renderer_type, window_properties.user_pixel_ratio);

  if (!view->engine()->IsRunning()) {
    if (!view->engine()->RunEngine()) {
      return nullptr;
    }
  }

  view->SendInitialGeometry();

  return HandleForView(view.release());
}

void* FlutterDesktopViewGetNativeHandle(FlutterDesktopViewRef view_ref) {
  flutter::FlutterTizenView* view = ViewFromHandle(view_ref);
  return view->tizen_view()->GetNativeHandle();
}

uint32_t FlutterDesktopViewGetResourceId(FlutterDesktopViewRef view_ref) {
  flutter::FlutterTizenView* view = ViewFromHandle(view_ref);
  return view->tizen_view()->GetResourceId();
}

void FlutterDesktopViewResize(FlutterDesktopViewRef view,
                              int32_t width,
                              int32_t height) {
  ViewFromHandle(view)->Resize(width, height);
}

void FlutterDesktopViewOnPointerEvent(FlutterDesktopViewRef view,
                                      FlutterDesktopPointerEventType type,
                                      double x,
                                      double y,
                                      size_t timestamp,
                                      int32_t device_id) {
  // TODO(swift-kim): Add support for mouse devices.
  FlutterPointerDeviceKind device_kind = kFlutterPointerDeviceKindTouch;
  FlutterPointerMouseButtons button = kFlutterPointerButtonMousePrimary;

  switch (type) {
    case FlutterDesktopPointerEventType::kMouseDown:
    default:
      ViewFromHandle(view)->OnPointerDown(x, y, button, timestamp, device_kind,
                                          device_id);
      break;
    case FlutterDesktopPointerEventType::kMouseUp:
      ViewFromHandle(view)->OnPointerUp(x, y, button, timestamp, device_kind,
                                        device_id);
      break;
    case FlutterDesktopPointerEventType::kMouseMove:
      ViewFromHandle(view)->OnPointerMove(x, y, timestamp, device_kind,
                                          device_id);
      break;
  }
}

void FlutterDesktopViewOnKeyEvent(FlutterDesktopViewRef view,
                                  const char* device_name,
                                  uint32_t device_class,
                                  uint32_t device_subclass,
                                  const char* key,
                                  const char* string,
                                  uint32_t modifiers,
                                  uint32_t scan_code,
                                  size_t timestamp,
                                  bool is_down) {
#ifdef NUI_SUPPORT
  if (auto* nui_view = dynamic_cast<flutter::TizenViewNui*>(
          ViewFromHandle(view)->tizen_view())) {
    nui_view->OnKey(device_name, device_class, device_subclass, key, string,
                    nullptr, modifiers, scan_code, timestamp, is_down);
  }
#else
  ViewFromHandle(view)->OnKey(key, string, nullptr, modifiers, scan_code,
                              device_name, is_down);
#endif
}

void FlutterDesktopViewSetFocus(FlutterDesktopViewRef view, bool focused) {
  if (auto* tizen_view = dynamic_cast<flutter::TizenView*>(
          ViewFromHandle(view)->tizen_view())) {
    tizen_view->SetFocus(focused);
  }
}

bool FlutterDesktopViewIsFocused(FlutterDesktopViewRef view) {
  if (auto* tizen_view = dynamic_cast<flutter::TizenView*>(
          ViewFromHandle(view)->tizen_view())) {
    return tizen_view->focused();
  }
  return false;
}

void FlutterDesktopRegisterViewFactory(
    FlutterDesktopPluginRegistrarRef registrar,
    const char* view_type,
    std::unique_ptr<PlatformViewFactory> view_factory) {
  registrar->engine->view()->platform_view_channel()->ViewFactories().insert(
      std::pair<std::string, std::unique_ptr<PlatformViewFactory>>(
          view_type, std::move(view_factory)));
}

// ========== Multi-view ==========

FlutterDesktopViewRef FlutterDesktopEngineAddView(
    FlutterDesktopEngineRef engine_ref,
    const FlutterDesktopWindowProperties& window_properties,
    FlutterDesktopAddViewCallback callback,
    void* user_data) {
  // Callers rely on |callback| firing exactly once even on failure paths so
  // they can release any per-request state (shared_ptr<MethodResult>,
  // AddViewContext, etc.) without racing against the async success path.
  // InvokeFailure centralizes that contract.
  auto InvokeFailure = [&](FlutterDesktopViewId view_id) {
    if (callback) {
      callback(/*added=*/false, view_id, user_data);
    }
    return static_cast<FlutterDesktopViewRef>(nullptr);
  };

  flutter::FlutterTizenEngine* engine = EngineFromHandle(engine_ref);
  if (!engine || !engine->IsRunning()) {
    FT_LOG(Error)
        << "FlutterDesktopEngineAddView requires a running engine; call "
           "FlutterDesktopViewCreateFromNewWindow first.";
    return InvokeFailure(FLUTTER_DESKTOP_IMPLICIT_VIEW_ID);
  }

  const FlutterViewId view_id = engine->AllocateViewId();

  flutter::TizenGeometry window_geometry = {
      window_properties.x, window_properties.y, window_properties.width,
      window_properties.height};

  auto window = std::make_unique<flutter::TizenWindowEcoreWl2>(
      window_geometry, window_properties.transparent,
      window_properties.focusable, window_properties.top_level,
      window_properties.pointing_device_support,
      window_properties.floating_menu_support, window_properties.window_handle,
      window_properties.renderer_type == kEVulkan);

  // Secondary-view constructor: engine is non-owning, RemoveView on destroy.
  // This constructor does NOT call engine->AddView(); the caller is
  // responsible so the async callback can be threaded all the way back to
  // the application.
  auto view = std::make_unique<flutter::FlutterTizenView>(
      view_id, std::move(window), engine, window_properties.renderer_type,
      window_properties.user_pixel_ratio);

  // Forward the async completion callback through engine->AddView. The embedder
  // API invokes AddView callbacks on an internal engine thread, so hop back to
  // the platform thread before touching Tizen objects or MethodResult state.
  auto* raw_view = view.release();
  const bool issued = engine->AddView(
      raw_view, [engine, callback, view_id, raw_view, user_data](bool added) {
        engine->PostPlatformTask(
            [callback, view_id, raw_view, user_data, added]() {
              if (callback) {
                callback(added, view_id, user_data);
              }
              if (!added) {
                delete raw_view;
              }
            });
      });
  if (!issued) {
    // engine->AddView has already reported failure through our lambda, so do
    // not report it again.
    return nullptr;
  }

  raw_view->SendInitialGeometry();
  return HandleForView(raw_view);
}

bool FlutterDesktopEngineRemoveView(FlutterDesktopEngineRef engine_ref,
                                    FlutterDesktopViewId view_id,
                                    FlutterDesktopRemoveViewCallback callback,
                                    void* user_data) {
  // Same single-callback-invocation contract as FlutterDesktopEngineAddView.
  auto InvokeFailure = [&]() {
    if (callback) {
      callback(/*removed=*/false, view_id, user_data);
    }
    return false;
  };

  flutter::FlutterTizenEngine* engine = EngineFromHandle(engine_ref);
  if (!engine) {
    return InvokeFailure();
  }
  if (view_id == FLUTTER_DESKTOP_IMPLICIT_VIEW_ID) {
    FT_LOG(Error) << "The implicit view cannot be removed via "
                     "FlutterDesktopEngineRemoveView.";
    return InvokeFailure();
  }

  flutter::FlutterTizenView* view = engine->GetView(view_id);
  if (!view) {
    return InvokeFailure();
  }

  // FlutterEngineRemoveView is asynchronous and the embedder API requires the
  // underlying surface to stay alive until the engine acknowledges removal.
  // Keep the FlutterTizenView alive until that callback, then delete it.
  return engine->RemoveView(
      view_id,
      [engine, view, callback, view_id, user_data](bool removed) {
        engine->PostPlatformTask(
            [engine, view, callback, view_id, user_data, removed]() {
              if (callback) {
                callback(removed, view_id, user_data);
              }
              if (removed) {
                engine->ReleaseRemovedView(view_id);
                delete view;
              }
            });
      },
      /*restore_on_failure=*/true,
      /*retain_renderable_until_release=*/true);
}

FlutterDesktopViewRef FlutterDesktopEngineGetView(
    FlutterDesktopEngineRef engine_ref,
    FlutterDesktopViewId view_id) {
  flutter::FlutterTizenEngine* engine = EngineFromHandle(engine_ref);
  if (!engine) {
    return nullptr;
  }
  return HandleForView(engine->GetView(view_id));
}

FlutterDesktopViewId FlutterDesktopViewGetId(FlutterDesktopViewRef view_ref) {
  flutter::FlutterTizenView* view = ViewFromHandle(view_ref);
  return view ? view->view_id() : FLUTTER_DESKTOP_IMPLICIT_VIEW_ID;
}

FlutterDesktopTextureRegistrarRef FlutterDesktopRegistrarGetTextureRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  return HandleForTextureRegistrar(registrar->engine->texture_registrar());
}

int64_t FlutterDesktopTextureRegistrarRegisterExternalTexture(
    FlutterDesktopTextureRegistrarRef texture_registrar,
    const FlutterDesktopTextureInfo* texture_info) {
  return TextureRegistrarFromHandle(texture_registrar)
      ->RegisterTexture(texture_info);
}

void FlutterDesktopTextureRegistrarUnregisterExternalTexture(
    FlutterDesktopTextureRegistrarRef texture_registrar,
    int64_t texture_id,
    void (*callback)(void* user_data),
    void* user_data) {
  TextureRegistrarFromHandle(texture_registrar)->UnregisterTexture(texture_id);
}

bool FlutterDesktopTextureRegistrarMarkExternalTextureFrameAvailable(
    FlutterDesktopTextureRegistrarRef texture_registrar,
    int64_t texture_id) {
  return TextureRegistrarFromHandle(texture_registrar)
      ->MarkTextureFrameAvailable(texture_id);
}

FlutterDesktopMessengerRef FlutterDesktopMessengerAddRef(
    FlutterDesktopMessengerRef messenger) {
  return messenger;
}

void FlutterDesktopMessengerRelease(FlutterDesktopMessengerRef messenger) {}

bool FlutterDesktopMessengerIsAvailable(FlutterDesktopMessengerRef messenger) {
  return messenger->engine != nullptr;
}

FlutterDesktopMessengerRef FlutterDesktopMessengerLock(
    FlutterDesktopMessengerRef messenger) {
  return messenger;
}

void FlutterDesktopMessengerUnlock(FlutterDesktopMessengerRef messenger) {}
