# Remove Ecore Dependency Plan

## Context
- Target branch: `feat/remove-ecore-wayland-rework`
- Requested design reference: PR `#155` (`https://github.com/flutter-tizen/embedder/pull/155`)
- Note: PR contents were not retrievable from this environment; this plan is based on current repo state and local history.

## Current Ecore/Ecore_* Usage Inventory

### Build dependencies
- `flutter/shell/platform/tizen/BUILD.gn`
  - Include dirs: `ecore-1`, `ecore-imf-1`, `ecore-input-1`, `ecore-wayland-1`, `ecore-wl2-1`
  - Linked libs: `ecore`, `ecore_imf`, `ecore_input`, `ecore_wl2`

### Runtime implementation usage
- `flutter/shell/platform/tizen/tizen_window_ecore_wl2.{h,cc}`
  - Uses `Ecore_Wl2` for display/window/EGL window, event handlers, input routing, rotation/configure, keygrab, cursor, policy/resource interactions.
- `flutter/shell/platform/tizen/tizen_input_method_context.{h,cc}`
  - Uses `Ecore_IMF` and `Ecore_Input` for text input panel state, preedit/commit callbacks, and key filtering.
- `flutter/shell/platform/tizen/tizen_event_loop.{h,cc}`
  - Uses `Ecore_Pipe` + `ecore_timer_add` for engine task wakeups.
- `flutter/shell/platform/tizen/tizen_vsync_waiter.{h,cc}`
  - Uses `Ecore_Thread` for vsync worker loop.
- `flutter/shell/platform/tizen/tizen_renderer_egl.cc`
  - Uses `ecore_wl2_egl_window_native_get`.
- `flutter/shell/platform/tizen/flutter_tizen_display_monitor.cc`
  - Uses `ecore_animator_frametime_get`.
- `flutter/shell/platform/tizen/tizen_clipboard.{h,cc}` (API != 6.0 path)
  - Uses Ecore-Wl2 clipboard wrappers/events.

### Test usage
- `flutter/shell/platform/tizen/flutter_tizen_engine_unittest.cc`
- `flutter/shell/platform/tizen/flutter_tizen_texture_registrar_unittests.cc`
  - both call `ecore_init()`.

## Replacement Strategy

### 1) Event loop and threading
- Replace `TizenEventLoop` ecore wakeups with GLib scheduling:
  - `g_main_context_invoke()` for immediate execution.
  - `g_timeout_add()` for delayed task expiration.
- Replace `TizenVsyncWaiter` `Ecore_Thread` with `std::thread` + `std::condition_variable` queue.

### 2) Windowing, input, and rendering target
- Keep `TizenWindowEcoreWl2` class/API name for compatibility, but replace internals with:
  - Core Wayland: `wl_display`, `wl_registry`, `wl_compositor`, `wl_surface`, `wl_seat`, `wl_output`.
  - Shell: `xdg_wm_base`, `xdg_surface`, `xdg_toplevel`.
  - EGL target: `wl_egl_window` (from `wayland-egl`) instead of `Ecore_Wl2_Egl_Window`.
  - Tizen extension protocol: `tizen_policy`, `tizen_indicator`, `tizen_keyrouter`, `tizen_surface`/`tizen_resource` for resource id.
- Replace Ecore event dispatch with Wayland callbacks (`wl_pointer`, `wl_keyboard`, `wl_touch`) and a GLib I/O watch for Wayland FD dispatch.
- Use xkbcommon to derive key symbol/string/modifier bits expected by current embedder logic.

### 3) IMF/Text input
- Replace `Ecore_IMF` with `wl_text_input` (`text-client-protocol`) implementation:
  - show/hide/reset, content type/purpose/capitalization settings.
  - preedit/commit callbacks mapped to existing `OnPreedit*` / `OnCommit` behavior.
  - input panel state and geometry tracking.
- Keep embedder-facing API behavior compatible; degrade gracefully when protocol objects are unavailable.

### 4) Clipboard (non-6.0 builds)
- Replace Ecore-Wl2 clipboard wrappers with direct `wl_data_device_manager`/`wl_data_device`/`wl_data_source`/`wl_data_offer` flow.
- Preserve current `SetData`, `GetData`, `HasStrings` API.

### 5) Display monitor and tests
- Replace `ecore_animator_frametime_get` usage with non-Ecore refresh-rate fallback (fixed/default when runtime query unavailable).
- Remove `ecore_init()` from unit tests.

## Build System Changes
- In `flutter/shell/platform/tizen/BUILD.gn`:
  - Remove ecore include paths and libs (`ecore`, `ecore_imf`, `ecore_input`, `ecore_wl2`).
  - Add required low-level libs/includes:
    - `wayland-egl`, `xkbcommon`, `glib-2.0`,
    - protocol client libs as needed (`xdg-shell-client`, `text-client`, existing `tizen-extension-client`).
  - Keep host/toolchain behavior cross-target-safe for `arm`, `arm64`, and `x86` include layout differences.

## Incremental Verification Plan
1. Build after event loop/vsync conversion.
2. Build after window/input/renderer transition.
3. Build after IMF transition.
4. Build after GN dependency cleanup.
5. Build target set for arm64:
   - `ninja -C out/tizen_arm64 flutter/shell/platform/tizen:flutter_tizen`
   - `ninja -C out/tizen_arm64 flutter/shell/platform/tizen:flutter_tizen_unittests`
6. Run test executables that are runnable in this environment and capture pass/fail logs; if execution is architecture-blocked, record exact failure and keep compile verification logs.

## Commit Plan
- Commit 1: event loop + vsync (remove `ecore` thread/timer usage).
- Commit 2: window/input/renderer direct Wayland refactor.
- Commit 3: IMF + clipboard + display monitor + test updates.
- Commit 4: GN/sysroot dependency cleanup and final fixes.
