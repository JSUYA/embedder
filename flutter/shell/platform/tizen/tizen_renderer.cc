// Copyright 2020 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/tizen/tizen_renderer.h"
#include "flutter/shell/platform/tizen/tizen_view_base.h"

namespace flutter {

TizenRenderer::TizenRenderer() {}

bool TizenRenderer::CreateSurface(TizenViewBase* view) {
  TizenGeometry geometry = view->GetGeometry();
  return CreateSurface(view->GetRenderTarget(), view->GetRenderTargetDisplay(),
                       geometry.width, geometry.height);
}

TizenRenderer::~TizenRenderer() = default;

}  // namespace flutter
