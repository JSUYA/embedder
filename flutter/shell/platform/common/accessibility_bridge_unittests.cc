// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "accessibility_bridge.h"

#include "gtest/gtest.h"

#include "test_accessibility_bridge.h"

namespace flutter {
namespace testing {
namespace {

FlutterSemanticsFlags kEmptyFlags = {};

FlutterSemanticsNode2 CreateSemanticsNode(
    int32_t id,
    const std::vector<int32_t>* children = nullptr) {
  return {
      .id = id,
      // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
      .flags__deprecated__ = static_cast<FlutterSemanticsFlag>(0),
      // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
      .actions = static_cast<FlutterSemanticsAction>(0),
      .text_selection_base = -1,
      .text_selection_extent = -1,
      .label = "",
      .hint = "",
      .value = "",
      .increased_value = "",
      .decreased_value = "",
      .child_count = children ? children->size() : 0,
      .children_in_traversal_order = children ? children->data() : nullptr,
      .tooltip = "",
      .flags2 = &kEmptyFlags,
  };
}

TEST(AccessibilityBridgeTest, IsSelectedAttribute) {
  auto bridge = std::make_shared<TestAccessibilityBridge>();

  std::vector<int32_t> children{1, 2};
  FlutterSemanticsNode2 root = CreateSemanticsNode(0, &children);
  FlutterSemanticsFlags root_flags = {
      .is_selected = kFlutterTristateNone,
  };
  root.flags2 = &root_flags;

  FlutterSemanticsNode2 selected = CreateSemanticsNode(1);
  FlutterSemanticsFlags selected_flags = {
      .is_selected = kFlutterTristateTrue,
  };
  selected.flags2 = &selected_flags;

  FlutterSemanticsNode2 unselected = CreateSemanticsNode(2);
  FlutterSemanticsFlags unselected_flags = {
      .is_selected = kFlutterTristateFalse,
  };
  unselected.flags2 = &unselected_flags;

  bridge->AddFlutterSemanticsNodeUpdate(root);
  bridge->AddFlutterSemanticsNodeUpdate(selected);
  bridge->AddFlutterSemanticsNodeUpdate(unselected);
  bridge->CommitUpdates();

  EXPECT_FALSE(bridge->GetFlutterPlatformNodeDelegateFromID(0)
                   .lock()
                   ->GetData()
                   .GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  EXPECT_TRUE(bridge->GetFlutterPlatformNodeDelegateFromID(1)
                  .lock()
                  ->GetData()
                  .GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  EXPECT_FALSE(bridge->GetFlutterPlatformNodeDelegateFromID(2)
                   .lock()
                   ->GetData()
                   .GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
}

}  // namespace
}  // namespace testing
}  // namespace flutter
