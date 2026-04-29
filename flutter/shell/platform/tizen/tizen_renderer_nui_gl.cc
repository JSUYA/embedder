// Copyright 2025 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/tizen/tizen_renderer_nui_gl.h"

namespace flutter {

TizenRendererNuiGL::TizenRendererNuiGL(TizenViewNui* view_nui,
                                       bool enable_impeller)
    : TizenRendererEgl(view_nui, enable_impeller) {}

TizenRendererNuiGL::~TizenRendererNuiGL() {}

bool TizenRendererNuiGL::OnPresent() {
  return TizenRendererEgl::OnPresent();
}

}  // namespace flutter
