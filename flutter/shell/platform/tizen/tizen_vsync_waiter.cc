// Copyright 2020 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tizen_vsync_waiter.h"

#include "flutter/shell/platform/tizen/flutter_tizen_engine.h"
#include "flutter/shell/platform/tizen/logger.h"

namespace flutter {

namespace {

}  // namespace

TizenVsyncWaiter::TizenVsyncWaiter(FlutterTizenEngine* engine) {
  tdm_client_ = std::make_shared<TdmClient>(engine);

  vblank_thread_ = std::thread([this]() { RunVblankLoop(); });
}

TizenVsyncWaiter::~TizenVsyncWaiter() {
  tdm_client_->OnEngineStop();

  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    quit_ = true;
  }
  queue_cv_.notify_all();

  if (vblank_thread_.joinable()) {
    vblank_thread_.join();
  }
}

void TizenVsyncWaiter::AsyncWaitForVsync(intptr_t baton) {
  EnqueueBaton(baton);
}

void TizenVsyncWaiter::EnqueueBaton(intptr_t baton) {
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (quit_) {
      return;
    }
    pending_batons_.push_back(baton);
  }
  queue_cv_.notify_one();
}

void TizenVsyncWaiter::RunVblankLoop() {
  // Keep the client alive for the lifetime of the vblank thread.
  auto client = tdm_client_;
  if (!client || !client->IsValid()) {
    FT_LOG(Error) << "Invalid tdm_client.";
    return;
  }

  while (true) {
    intptr_t baton = 0;
    {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      queue_cv_.wait(lock, [this]() {
        return quit_ || !pending_batons_.empty();
      });

      if (quit_) {
        break;
      }

      baton = pending_batons_.front();
      pending_batons_.pop_front();
    }

    client->AwaitVblank(baton);
  }
}

TdmClient::TdmClient(FlutterTizenEngine* engine) {
  tdm_error ret;
  client_ = tdm_client_create(&ret);
  if (ret != TDM_ERROR_NONE) {
    FT_LOG(Error) << "Failed to create a TDM client.";
    return;
  }

  output_ = tdm_client_get_output(client_, const_cast<char*>("default"), &ret);
  if (ret != TDM_ERROR_NONE) {
    FT_LOG(Error) << "Could not obtain the default client output.";
    return;
  }

  vblank_ = tdm_client_output_create_vblank(output_, &ret);
  if (ret != TDM_ERROR_NONE) {
    FT_LOG(Error) << "Failed to create a vblank object.";
    return;
  }
  tdm_client_vblank_set_enable_fake(vblank_, 1);

  engine_ = engine;
}

TdmClient::~TdmClient() {
  if (vblank_) {
    tdm_client_vblank_destroy(vblank_);
    vblank_ = nullptr;
  }
  output_ = nullptr;
  if (client_) {
    tdm_client_destroy(client_);
    client_ = nullptr;
  }
}

bool TdmClient::IsValid() {
  return vblank_ && client_;
}

void TdmClient::OnEngineStop() {
  std::lock_guard<std::mutex> lock(engine_mutex_);
  engine_ = nullptr;
}

void TdmClient::AwaitVblank(intptr_t baton) {
  if (!IsValid()) {
    return;
  }

  {
    // Protect baton_ as it's read from the vblank callback.
    std::lock_guard<std::mutex> lock(engine_mutex_);
    baton_ = baton;
  }

  tdm_error ret = tdm_client_vblank_wait(vblank_, 1, VblankCallback, this);
  if (ret != TDM_ERROR_NONE) {
    FT_LOG(Error) << "tdm_client_vblank_wait failed with error: " << ret;
    return;
  }
  tdm_client_handle_events(client_);
}

void TdmClient::VblankCallback(tdm_client_vblank* vblank,
                               tdm_error error,
                               unsigned int sequence,
                               unsigned int tv_sec,
                               unsigned int tv_usec,
                               void* user_data) {
  auto* self = static_cast<TdmClient*>(user_data);
  if (!self) {
    return;
  }

  std::lock_guard<std::mutex> lock(self->engine_mutex_);
  if (self->engine_) {
    uint64_t frame_start_time_nanos = tv_sec * 1e9 + tv_usec * 1e3;
    uint64_t frame_target_time_nanos = frame_start_time_nanos + 16.6 * 1e6;
    self->engine_->OnVsync(self->baton_, frame_start_time_nanos,
                           frame_target_time_nanos);
  }
}

}  // namespace flutter
