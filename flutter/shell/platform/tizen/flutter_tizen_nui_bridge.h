// Copyright 2026 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_FLUTTER_TIZEN_NUI_BRIDGE_H_
#define EMBEDDER_FLUTTER_TIZEN_NUI_BRIDGE_H_

#include "flutter/shell/platform/tizen/public/flutter_tizen.h"

namespace flutter {
class TizenViewBase;
}  // namespace flutter

struct FlutterTizenNuiBridge {
  FlutterDesktopViewRef (*create_view)(
      const FlutterDesktopViewProperties& view_properties,
      FlutterDesktopEngineRef engine,
      flutter::TizenViewBase* tizen_view);
};

#endif  // EMBEDDER_FLUTTER_TIZEN_NUI_BRIDGE_H_
