// Copyright 2025 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/tizen/tizen_nui_backend_loader.h"

#include <dlfcn.h>

#include "flutter/shell/platform/tizen/logger.h"

namespace flutter {

namespace {

constexpr char kNuiBackendLibrary[] = "libflutter_tizen_nui.so";
constexpr char kNuiBackendGetSymbol[] = "FlutterTizenNuiBackendGet";

using FlutterTizenNuiBackendGetFn = const FlutterTizenNuiBackend* (*)(void);

const FlutterTizenNuiBackend* LoadBackend() {
  // RTLD_NOW forces the DALi dependencies of the backend library to be resolved
  // now, so that a missing DALi installation is reported here rather than
  // surfacing as an unresolved symbol later.
  void* handle = dlopen(kNuiBackendLibrary, RTLD_NOW | RTLD_LOCAL);
  if (!handle) {
    FT_LOG(Error) << "Failed to load the NUI/DALi backend ("
                  << kNuiBackendLibrary << "): " << dlerror();
    return nullptr;
  }

  auto backend_get = reinterpret_cast<FlutterTizenNuiBackendGetFn>(
      dlsym(handle, kNuiBackendGetSymbol));
  if (!backend_get) {
    FT_LOG(Error) << "Failed to resolve " << kNuiBackendGetSymbol << ": "
                  << dlerror();
    dlclose(handle);
    return nullptr;
  }

  // The handle is intentionally leaked: the backend is used for the lifetime of
  // the process and unloading DALi would be unsafe once initialized.
  return backend_get();
}

}  // namespace

const FlutterTizenNuiBackend* GetTizenNuiBackend() {
  // Thread-safe, one-time initialization (C++11 static local semantics).
  static const FlutterTizenNuiBackend* backend = LoadBackend();
  return backend;
}

}  // namespace flutter
