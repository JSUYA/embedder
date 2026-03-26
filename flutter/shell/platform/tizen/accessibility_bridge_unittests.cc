// Copyright 2026 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/common/test_accessibility_bridge.h"

#include "flutter/third_party/accessibility/ax/ax_action_data.h"
#include "gtest/gtest.h"

namespace flutter {
namespace {

FlutterTransformation IdentityTransform() {
  return FlutterTransformation{
      1.0, 0.0, 0.0,
      0.0, 1.0, 0.0,
      0.0, 0.0, 1.0,
  };
}

FlutterSemanticsFlags MakeFlags(bool blocked) {
  FlutterSemanticsFlags flags = {};
  flags.struct_size = sizeof(FlutterSemanticsFlags);
  flags.is_accessibility_focus_blocked = blocked;
  return flags;
}

FlutterSemanticsNode2 MakeNode(int32_t id, FlutterSemanticsFlags* flags) {
  FlutterSemanticsNode2 node = {};
  node.struct_size = sizeof(FlutterSemanticsNode2);
  node.id = id;
  node.actions = kFlutterSemanticsActionTap;
  node.label = "target";
  node.text_direction = kFlutterTextDirectionLTR;
  node.rect = FlutterRect{0.0, 0.0, 10.0, 10.0};
  node.transform = IdentityTransform();
  node.platform_view_id = -1;
  node.flags2 = flags;
  return node;
}

std::shared_ptr<FlutterPlatformNodeDelegate> GetDelegate(
    TestAccessibilityBridge& bridge,
    int32_t node_id) {
  auto delegate = bridge.GetFlutterPlatformNodeDelegateFromID(node_id).lock();
  EXPECT_NE(delegate, nullptr);
  return delegate;
}

}  // namespace

TEST(AccessibilityBridgeTest, BlockedNodeDoesNotBecomeFocusable) {
  TestAccessibilityBridge bridge;
  auto flags = MakeFlags(true);
  auto node = MakeNode(1, &flags);

  bridge.AddFlutterSemanticsNodeUpdate(node);
  bridge.CommitUpdates();

  auto* ax_node = bridge.GetNodeFromTree(1);
  ASSERT_NE(ax_node, nullptr);
  EXPECT_FALSE(ax_node->data().HasState(ax::mojom::State::kFocusable));
}

TEST(AccessibilityBridgeTest, BlockedNodeIsSkippedInUnignoredTree) {
  TestAccessibilityBridge bridge;
  auto root_flags = MakeFlags(false);
  auto blocked_flags = MakeFlags(true);
  auto visible_flags = MakeFlags(false);
  const int32_t child_ids[] = {2, 3};

  auto root = MakeNode(1, &root_flags);
  root.child_count = 2;
  root.children_in_traversal_order = child_ids;
  root.children_in_hit_test_order = child_ids;

  auto blocked_child = MakeNode(2, &blocked_flags);
  auto visible_child = MakeNode(3, &visible_flags);

  bridge.AddFlutterSemanticsNodeUpdate(root);
  bridge.AddFlutterSemanticsNodeUpdate(blocked_child);
  bridge.AddFlutterSemanticsNodeUpdate(visible_child);
  bridge.CommitUpdates();

  auto* root_node = bridge.GetNodeFromTree(root.id);
  ASSERT_NE(root_node, nullptr);
  EXPECT_EQ(root_node->GetUnignoredChildCount(), 1u);
  EXPECT_EQ(root_node->GetUnignoredChildAtIndex(0)->id(), visible_child.id);

  auto* blocked_node = bridge.GetNodeFromTree(blocked_child.id);
  ASSERT_NE(blocked_node, nullptr);
  EXPECT_TRUE(blocked_node->data().IsIgnored());
}

TEST(AccessibilityBridgeTest, BlockedNodeRejectsAccessibilityFocusAction) {
  TestAccessibilityBridge bridge;
  auto flags = MakeFlags(true);
  auto node = MakeNode(1, &flags);

  bridge.AddFlutterSemanticsNodeUpdate(node);
  bridge.CommitUpdates();

  auto delegate = GetDelegate(bridge, node.id);
  ASSERT_NE(delegate, nullptr);

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kFocus;

  EXPECT_FALSE(delegate->AccessibilityPerformAction(action_data));
  EXPECT_TRUE(bridge.performed_actions.empty());
  EXPECT_EQ(bridge.GetLastFocusedId(), ui::AXNode::kInvalidAXID);
}

TEST(AccessibilityBridgeTest, UnblockedNodeAcceptsAccessibilityFocusAction) {
  TestAccessibilityBridge bridge;
  auto flags = MakeFlags(false);
  auto node = MakeNode(1, &flags);

  bridge.AddFlutterSemanticsNodeUpdate(node);
  bridge.CommitUpdates();

  auto delegate = GetDelegate(bridge, node.id);
  ASSERT_NE(delegate, nullptr);

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kFocus;

  EXPECT_TRUE(delegate->AccessibilityPerformAction(action_data));
  ASSERT_EQ(bridge.performed_actions.size(), 1u);
  EXPECT_EQ(
      bridge.performed_actions.front(),
      FlutterSemanticsAction::kFlutterSemanticsActionDidGainAccessibilityFocus);
  EXPECT_EQ(bridge.GetLastFocusedId(), node.id);
}
