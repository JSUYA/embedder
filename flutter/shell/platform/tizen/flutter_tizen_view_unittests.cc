// Copyright 2026 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/tizen/flutter_tizen_view.h"

#include <Ecore.h>

#include <vector>

#include "flutter/shell/platform/embedder/test_utils/proc_table_replacement.h"
#include "flutter/shell/platform/tizen/testing/engine_modifier.h"
#include "flutter/shell/platform/tizen/tizen_window.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {
namespace {

class TestTizenView : public TizenWindow {
 public:
  TestTizenView() {
    input_method_context_ = std::make_unique<TizenInputMethodContext>(0);
  }

  void* GetRenderTarget() override { return nullptr; }
  void* GetNativeHandle() override { return nullptr; }
  uintptr_t GetWindowId() override { return 0; }
  TizenGeometry GetGeometry() override { return {0, 0, 100, 100}; }
  bool SetGeometry(TizenGeometry geometry) override { return true; }
  int32_t GetDpi() override { return 160; }
  uint32_t GetResourceId() override { return 0; }
  void UpdateFlutterCursor(const std::string& kind) override {}
  void Show() override {}
  int32_t GetRotation() override { return 0; }
  void SetPreferredOrientations(const std::vector<int>& rotations) override {}
  void* GetRenderTargetDisplay() override { return nullptr; }
  TizenGeometry GetScreenGeometry() override { return GetGeometry(); }
  void BindKeys(const std::vector<std::string>& keys) override {}
  void ActivateWindow() override { activated = true; }
  void RaiseWindow() override {}
  void LowerWindow() override {}

  bool activated = false;
};

TEST(FlutterTizenViewTest, SendsPointerPressure) {
  ecore_init();
  {
    FlutterDesktopEngineProperties properties = {};
    properties.assets_path = "/foo/flutter_assets";
    properties.icu_data_path = "/foo/icudtl.dat";
    properties.aot_library_path = "/foo/libapp.so";

    FlutterProjectBundle project(properties);
    auto engine = std::make_unique<FlutterTizenEngine>(project);
    std::vector<FlutterPointerEvent> events;
    EngineModifier modifier(engine.get());
    modifier.embedder_api().SendPointerEvent = MOCK_ENGINE_PROC(
        SendPointerEvent,
        ([&events](auto engine, const FlutterPointerEvent* event,
                   size_t count) {
          EXPECT_EQ(count, 1u);
          events.push_back(*event);
          return kSuccess;
        }));

    FlutterTizenView view(kImplicitViewId, std::make_unique<TestTizenView>(),
                          std::move(engine), kEVulkan);
    view.OnPointerDown(10, 20, kFlutterPointerButtonMousePrimary, 123,
                       kFlutterPointerDeviceKindStylus, 7, 0.75);
    view.OnPointerMove(11, 21, 124, kFlutterPointerDeviceKindStylus, 7, 1.25);

    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[0].phase, kAdd);
    EXPECT_EQ(events[1].phase, kDown);
    EXPECT_EQ(events[2].phase, kMove);
    for (size_t i = 0; i < 2; ++i) {
      EXPECT_DOUBLE_EQ(events[i].pressure, 0.75);
      EXPECT_DOUBLE_EQ(events[i].pressure_min, 0.0);
      EXPECT_DOUBLE_EQ(events[i].pressure_max, 1.0);
    }
    EXPECT_DOUBLE_EQ(events[2].pressure, 1.25);
    EXPECT_DOUBLE_EQ(events[2].pressure_max, 1.25);
  }

  ecore_shutdown();
}

TEST(FlutterTizenViewTest, SendsAndRequestsViewFocus) {
  ecore_init();
  {
    FlutterDesktopEngineProperties properties = {};
    properties.assets_path = "/foo/flutter_assets";
    properties.icu_data_path = "/foo/icudtl.dat";
    properties.aot_library_path = "/foo/libapp.so";

    FlutterProjectBundle project(properties);
    auto engine = std::make_unique<FlutterTizenEngine>(project);
    FlutterViewFocusEvent sent_event = {};
    EngineModifier modifier(engine.get());
    modifier.SetEngine(reinterpret_cast<FLUTTER_API_SYMBOL(FlutterEngine)>(1));
    modifier.embedder_api().SendViewFocusEvent = MOCK_ENGINE_PROC(
        SendViewFocusEvent,
        ([&sent_event](auto engine, const FlutterViewFocusEvent* event) {
          sent_event = *event;
          return kSuccess;
        }));
    modifier.embedder_api().Shutdown = [](auto engine) { return kSuccess; };

    auto tizen_view = std::make_unique<TestTizenView>();
    TestTizenView* tizen_view_ptr = tizen_view.get();
    FlutterTizenView view(kImplicitViewId, std::move(tizen_view),
                          std::move(engine), kEVulkan);

    view.OnFocus(kFocused);
    EXPECT_EQ(sent_event.struct_size, sizeof(FlutterViewFocusEvent));
    EXPECT_EQ(sent_event.view_id, kImplicitViewId);
    EXPECT_EQ(sent_event.state, kFocused);
    EXPECT_EQ(sent_event.direction, kUndefined);

    FlutterViewFocusChangeRequest request = {
        .struct_size = sizeof(FlutterViewFocusChangeRequest),
        .view_id = kImplicitViewId,
        .state = kUnfocused,
        .direction = kUndefined,
    };
    view.OnFocusChangeRequest(request);
    EXPECT_FALSE(tizen_view_ptr->activated);

    request.state = kFocused;
    view.OnFocusChangeRequest(request);
    EXPECT_TRUE(tizen_view_ptr->activated);
  }
  ecore_shutdown();
}

}  // namespace
}  // namespace testing
}  // namespace flutter
