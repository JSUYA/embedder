// Copyright 2022 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "feedback_manager.h"

#include <dlfcn.h>

#include "flutter/shell/platform/tizen/logger.h"

namespace flutter {

namespace {

constexpr char kFeedbackLibrary[] = "libfeedback.so.0";

template <typename T>
T ResolveSymbol(void* library, const char* name) {
  return reinterpret_cast<T>(dlsym(library, name));
}

}  // namespace

FeedbackManager::FeedbackManager() {
  library_ = dlopen(kFeedbackLibrary, RTLD_LAZY | RTLD_LOCAL);
  if (!library_) {
    FT_LOG(Warn) << "Failed to load feedback library: " << dlerror();
    return;
  }

  feedback_initialize_ =
      ResolveSymbol<FeedbackInitializeFn>(library_, "feedback_initialize");
  feedback_deinitialize_ =
      ResolveSymbol<FeedbackDeinitializeFn>(library_, "feedback_deinitialize");
  feedback_play_type_ =
      ResolveSymbol<FeedbackPlayTypeFn>(library_, "feedback_play_type");
  if (!feedback_initialize_ || !feedback_deinitialize_ ||
      !feedback_play_type_) {
    FT_LOG(Warn) << "Failed to resolve feedback symbols.";
    dlclose(library_);
    library_ = nullptr;
    return;
  }

  int ret = feedback_initialize_();
  if (ret != FEEDBACK_ERROR_NONE) {
    FT_LOG(Error) << "feedback_initialize() failed with error: "
                  << get_error_message(ret);
    return;
  }
  initialized_ = true;
}

FeedbackManager::~FeedbackManager() {
  if (initialized_) {
    feedback_deinitialize_();
  }
  if (library_) {
    dlclose(library_);
  }
}

void FeedbackManager::Play(feedback_type_e type, feedback_pattern_e pattern) {
  if (!initialized_) {
    return;
  }
  int ret = feedback_play_type_(type, pattern);
  if (ret == FEEDBACK_ERROR_PERMISSION_DENIED) {
    FT_LOG(Error)
        << "Permission denied. Add the http://tizen.org/privilege/haptic "
           "privilege to tizen-manifest.xml to use haptic feedbacks.";
  } else if (ret != FEEDBACK_ERROR_NONE) {
    FT_LOG(Error) << "feedback_play_type() failed with error: "
                  << get_error_message(ret);
  }
}

}  // namespace flutter
