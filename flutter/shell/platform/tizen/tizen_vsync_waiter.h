// Copyright 2020 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_TIZEN_VSYNC_WAITER_H_
#define EMBEDDER_TIZEN_VSYNC_WAITER_H_

#include <tdm_client.h>

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

class FlutterTizenEngine;

class TdmClient {
 public:
  TdmClient(FlutterTizenEngine* engine);
  virtual ~TdmClient();

  bool IsValid();
  void OnEngineStop();
  void AwaitVblank(intptr_t baton);

 private:
  static void VblankCallback(tdm_client_vblank* vblank,
                             tdm_error error,
                             unsigned int sequence,
                             unsigned int tv_sec,
                             unsigned int tv_usec,
                             void* user_data);

  FlutterTizenEngine* engine_ = nullptr;
  std::mutex engine_mutex_;

  tdm_client* client_ = nullptr;
  tdm_client_output* output_ = nullptr;
  tdm_client_vblank* vblank_ = nullptr;

  intptr_t baton_ = 0;
};

class TizenVsyncWaiter {
 public:
  TizenVsyncWaiter(FlutterTizenEngine* engine);
  virtual ~TizenVsyncWaiter();

 void AsyncWaitForVsync(intptr_t baton);

 private:
  void EnqueueVsyncRequest(intptr_t baton);

  void RunVblankLoop();

  std::shared_ptr<TdmClient> tdm_client_;
  std::thread vblank_thread_;
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  std::deque<intptr_t> batons_;
  bool stop_requested_ = false;
};

}  // namespace flutter

#endif  // EMBEDDER_TIZEN_VSYNC_WAITER_H_
