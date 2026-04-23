// Copyright 2020 Samsung Electronics Co., Ltd. All rights reserved.
// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_TIZEN_PUBLIC_FLUTTER_TIZEN_H_
#define FLUTTER_SHELL_PLATFORM_TIZEN_PUBLIC_FLUTTER_TIZEN_H_

#include <stddef.h>
#include <stdint.h>

#include "flutter_export.h"
#include "flutter_messenger.h"
#include "flutter_plugin_registrar.h"

#if defined(__cplusplus)
extern "C" {
#endif

// Opaque reference to a Flutter engine instance.
struct FlutterDesktopEngine;
typedef struct FlutterDesktopEngine* FlutterDesktopEngineRef;

// Opaque reference to a Flutter view instance.
struct FlutterDesktopView;
typedef struct FlutterDesktopView* FlutterDesktopViewRef;

// Identifier for a Flutter view within an engine. Matches FlutterViewId in
// the Flutter embedder API. The implicit (primary) view has id 0.
typedef int64_t FlutterDesktopViewId;

// The id of the implicit view that is created together with an engine. Match
// the FlutterEngine convention: view id 0 is the implicit view.
#define FLUTTER_DESKTOP_IMPLICIT_VIEW_ID ((FlutterDesktopViewId)0)

// Callback invoked when an asynchronous AddView operation completes.
//
// |added| is true when the Flutter engine has accepted the new view and
// |view_id| contains the id assigned to it. On failure |added| is false
// and the view has not been registered (any platform-side state for that
// id is rolled back before this callback fires). |user_data| is the value
// passed to FlutterDesktopEngineAddView.
typedef void (*FlutterDesktopAddViewCallback)(bool added,
                                              FlutterDesktopViewId view_id,
                                              void* user_data);

// Callback invoked when an asynchronous RemoveView operation completes.
typedef void (*FlutterDesktopRemoveViewCallback)(bool removed,
                                                 FlutterDesktopViewId view_id,
                                                 void* user_data);

typedef enum {
  // The renderer based on EGL.
  kEGL,
  // The renderer based on Vulkan.
  kEVulkan
} FlutterDesktopRendererType;

typedef enum {
  kMouseDown,
  kMouseUp,
  kMouseMove,
} FlutterDesktopPointerEventType;

typedef enum {
  // No external output.
  kNone,
  // Display to the HDMI external output.
  kHDMI,
} FlutterDesktopExternalOutputType;

// Configures the thread policy for running the UI isolate.
typedef enum {
  // Default value. Currently run the UI isolate on platform thread.
  kDefault,
  // Run the UI isolate on platform thread.
  kRunOnPlatformThread,
  // Run the UI isolate on a separate thread.
  kRunOnSeparateThread,
} FlutterDesktopUIThreadPolicy;

// Properties for configuring the initial settings of a Flutter window.
typedef struct {
  // The x-coordinate of the top left corner of the window.
  int32_t x;
  // The y-coordinate of the top left corner of the window.
  int32_t y;
  // The width of the window, or the maximum width if the value is zero.
  int32_t width;
  // The height of the window, or the maximum height if the value is zero.
  int32_t height;
  // Whether the window should have a transparent background or not.
  bool transparent;
  // Whether the window should be focusable or not.
  bool focusable;
  // Whether the window should be on top layer or not.
  bool top_level;
  // The renderer type of the engine.
  FlutterDesktopRendererType renderer_type;
  // The user-defined pixel ratio.
  double user_pixel_ratio;
  // The precreated window handle.
  void* window_handle;
  // Whether the app should be supported pointing device(mouse) or not.
  bool pointing_device_support;
  // Whether the app should be supported floating menu or not.
  bool floating_menu_support;
} FlutterDesktopWindowProperties;

// Properties for configuring the initial settings of a Flutter view.
typedef struct {
  // The width of the view, or the maximum width if the value is zero.
  int32_t width;
  // The height of the view, or the maximum height if the value is zero.
  int32_t height;
} FlutterDesktopViewProperties;

// Properties for configuring a Flutter engine instance.
typedef struct {
  // The path to the flutter_assets folder for the application to be run.
  const char* assets_path;
  // The path to the icudtl.dat file for the version of Flutter you are using.
  const char* icu_data_path;
  // The path to the AOT libary file for your application, if any.
  const char* aot_library_path;
  // The switches to pass to the Flutter engine.
  //
  // See: https://github.com/flutter/engine/blob/master/shell/common/switches.h
  // for details. Not all arguments will apply.
  const char** switches;
  // The number of elements in |switches|.
  size_t switches_count;
  // The optional entrypoint in the Dart project. If the value is null or
  // empty, defaults to main().
  const char* entrypoint;
  // Number of elements in the array passed in as dart_entrypoint_argv.
  int dart_entrypoint_argc;
  // Array of Dart entrypoint arguments. This is deep copied during the call
  // to FlutterDesktopRunEngine.
  const char** dart_entrypoint_argv;
  // Policy for the thread that runs UI isolate.
  FlutterDesktopUIThreadPolicy ui_thread_policy;
} FlutterDesktopEngineProperties;

// ========== Engine ==========

// Creates a Flutter engine with the given properties.
FLUTTER_EXPORT FlutterDesktopEngineRef FlutterDesktopEngineCreate(
    const FlutterDesktopEngineProperties& engine_properties);

// Runs an instance of a Flutter engine with the given properties.
//
// If view is not specified, the engine is run in headless mode.
FLUTTER_EXPORT bool FlutterDesktopEngineRun(
    const FlutterDesktopEngineRef engine);

// Shuts down the given engine instance.
//
// |engine| is no longer valid after this call.
FLUTTER_EXPORT void FlutterDesktopEngineShutdown(
    FlutterDesktopEngineRef engine);

// Returns the plugin registrar handle for the plugin with the given name.
//
// The name must be unique across the application.
FLUTTER_EXPORT FlutterDesktopPluginRegistrarRef
FlutterDesktopEngineGetPluginRegistrar(FlutterDesktopEngineRef engine,
                                       const char* plugin_name);

// Returns the messenger associated with the engine.
FLUTTER_EXPORT FlutterDesktopMessengerRef
FlutterDesktopEngineGetMessenger(FlutterDesktopEngineRef engine);

// Posts an app control to the engine instance.
FLUTTER_EXPORT void FlutterDesktopEngineNotifyAppControl(
    FlutterDesktopEngineRef engine,
    void* app_control);

// Posts a locale change notification to the engine instance.
FLUTTER_EXPORT void FlutterDesktopEngineNotifyLocaleChange(
    FlutterDesktopEngineRef engine);

// Posts a low memory notification to the engine instance.
FLUTTER_EXPORT void FlutterDesktopEngineNotifyLowMemoryWarning(
    FlutterDesktopEngineRef engine);

// Notifies the engine that the app is in an inactive state and not receiving
// user input.
FLUTTER_EXPORT void FlutterDesktopEngineNotifyAppIsInactive(
    FlutterDesktopEngineRef engine);

// Notifies the engine that the app is visible and responding to user input.
FLUTTER_EXPORT void FlutterDesktopEngineNotifyAppIsResumed(
    FlutterDesktopEngineRef engine);

// Notifies the engine that the app is not currently visible to the user, not
// responding to user input, and running in the background.
FLUTTER_EXPORT void FlutterDesktopEngineNotifyAppIsPaused(
    FlutterDesktopEngineRef engine);

// Notifies the engine that the engine is detached from any host views.
FLUTTER_EXPORT void FlutterDesktopEngineNotifyAppIsDetached(
    FlutterDesktopEngineRef engine);

// ========== View ==========

// Creates a view that hosts and displays the given engine instance.
//
// The first call on a fresh engine creates the implicit view (view id 0)
// and starts the engine. This function is retained for source-level
// backward compatibility; for secondary (multi-view) windows prefer
// FlutterDesktopEngineAddView below.
FLUTTER_EXPORT FlutterDesktopViewRef FlutterDesktopViewCreateFromNewWindow(
    const FlutterDesktopWindowProperties& window_properties,
    FlutterDesktopEngineRef engine);

// Creates a view that hosts and displays the given engine instance.
//
// The type of |image_view| must be Dali::Toolkit::ImageView*.
// The type of |native_image_queue| must be Dali::NativeImageSourceQueue*.
// @warning This API is a work-in-progress and may change.
FLUTTER_EXPORT FlutterDesktopViewRef FlutterDesktopViewCreateFromImageView(
    const FlutterDesktopViewProperties& view_properties,
    FlutterDesktopEngineRef engine,
    void* image_view,
    void* native_image_queue,
    int32_t default_window_id);

// Destroys the view.
//
// The engine owned by the view will also be shut down implicitly.
// @warning This API is a work-in-progress and may change.
FLUTTER_EXPORT void FlutterDesktopViewDestroy(FlutterDesktopViewRef view);

// Returns a native UI toolkit handle for manipulation in host application.
//
// Cast the returned void*
// - window ecore wl2   : to Ecore_Wl2_Window*
// @warning This API is a work-in-progress and may change.
FLUTTER_EXPORT void* FlutterDesktopViewGetNativeHandle(
    FlutterDesktopViewRef view);

// Returns the resource id of current window.
FLUTTER_EXPORT uint32_t
FlutterDesktopViewGetResourceId(FlutterDesktopViewRef view);

// Resizes the view.
// @warning This API is a work-in-progress and may change.
FLUTTER_EXPORT void FlutterDesktopViewResize(FlutterDesktopViewRef view,
                                             int32_t width,
                                             int32_t height);

FLUTTER_EXPORT void FlutterDesktopViewOnPointerEvent(
    FlutterDesktopViewRef view,
    FlutterDesktopPointerEventType type,
    double x,
    double y,
    size_t timestamp,
    int32_t device_id);

FLUTTER_EXPORT void FlutterDesktopViewOnKeyEvent(FlutterDesktopViewRef view,
                                                 const char* device_name,
                                                 uint32_t device_class,
                                                 uint32_t device_subclass,
                                                 const char* key,
                                                 const char* string,
                                                 uint32_t modifiers,
                                                 uint32_t scan_code,
                                                 size_t timestamp,
                                                 bool is_down);

FLUTTER_EXPORT void FlutterDesktopViewSetFocus(FlutterDesktopViewRef view,
                                               bool focused);

FLUTTER_EXPORT bool FlutterDesktopViewIsFocused(FlutterDesktopViewRef view);

// ========== Multi-view ==========

// Adds a new top-level window/view to an already-running engine.
//
// The engine must have been started via FlutterDesktopViewCreateFromNewWindow
// (which creates the implicit view). This function creates a fresh Tizen
// window and registers it with the engine as a secondary view. The call is
// asynchronous: |callback| is invoked on the platform thread once the Flutter
// engine has acknowledged the registration. The returned |FlutterDesktopViewRef|
// is valid immediately and can be used synchronously, but the Dart side will
// not observe the new view until the callback fires.
//
// Returns nullptr if the engine is not running or the platform window could
// not be created; in that case |callback| is not invoked.
//
// NOTE: Until the multi-view compositor work lands, secondary views are
// registered with the Flutter framework (so PlatformDispatcher.views sees
// them) but their contents are not rendered. Rendering secondary views
// requires a FlutterCompositor that routes layers by view id.
FLUTTER_EXPORT FlutterDesktopViewRef FlutterDesktopEngineAddView(
    FlutterDesktopEngineRef engine,
    const FlutterDesktopWindowProperties& window_properties,
    FlutterDesktopAddViewCallback callback,
    void* user_data);

// Removes a previously-added secondary view from the engine.
//
// |view_id| must refer to a view created through FlutterDesktopEngineAddView.
// The implicit view (FLUTTER_DESKTOP_IMPLICIT_VIEW_ID) cannot be removed
// this way; it is released together with the engine itself.
// |callback| is invoked once the Flutter engine has unregistered the view.
// After the callback fires the corresponding FlutterDesktopViewRef returned
// from FlutterDesktopEngineAddView must no longer be used.
FLUTTER_EXPORT bool FlutterDesktopEngineRemoveView(
    FlutterDesktopEngineRef engine,
    FlutterDesktopViewId view_id,
    FlutterDesktopRemoveViewCallback callback,
    void* user_data);

// Returns the view registered with |engine| under |view_id|, or nullptr if
// no such view exists.
FLUTTER_EXPORT FlutterDesktopViewRef FlutterDesktopEngineGetView(
    FlutterDesktopEngineRef engine,
    FlutterDesktopViewId view_id);

// Returns the id of |view|, or FLUTTER_DESKTOP_IMPLICIT_VIEW_ID if |view| is
// the implicit view.
FLUTTER_EXPORT FlutterDesktopViewId
FlutterDesktopViewGetId(FlutterDesktopViewRef view);

// ========== Plugin Registrar (extensions) ==========

// Returns the view associated with this registrar's engine instance.
FLUTTER_EXPORT FlutterDesktopViewRef FlutterDesktopPluginRegistrarGetView(
    FlutterDesktopPluginRegistrarRef registrar);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif  // FLUTTER_SHELL_PLATFORM_TIZEN_PUBLIC_FLUTTER_TIZEN_H_
