// Copyright 2025 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_TIZEN_NUI_BACKEND_LOADER_H_
#define EMBEDDER_TIZEN_NUI_BACKEND_LOADER_H_

#include "flutter/shell/platform/tizen/tizen_nui_backend.h"

namespace flutter {

// Lazily loads the NUI/DALi backend shared library (libflutter_tizen_nui.so)
// and returns its vtable.
//
// The library is loaded once, on the first call, and the result is cached. This
// is the single point where the presence of DALi is validated at runtime:
// loading with RTLD_NOW forces resolution of the library's DALi dependencies,
// so a missing or incompatible DALi installation is detected here.
//
// Returns nullptr - and logs the reason - if the backend or its dependencies
// are unavailable. Callers that need the NUI feature MUST check for nullptr
// before using the returned vtable.
const FlutterTizenNuiBackend* GetTizenNuiBackend();

}  // namespace flutter

#endif  // EMBEDDER_TIZEN_NUI_BACKEND_LOADER_H_
