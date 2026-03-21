// Copyright 2020 Samsung Electronics Co., Ltd. All rights reserved.
// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tizen_event_loop.h"

#include <algorithm>
#include <utility>

namespace flutter {

TizenEventLoop::TizenEventLoop(std::thread::id main_thread_id,
                               CurrentTimeProc get_current_time,
                               TaskExpiredCallback on_task_expired)
    : main_thread_id_(main_thread_id),
      get_current_time_(get_current_time),
      on_task_expired_(std::move(on_task_expired)) {}

TizenEventLoop::~TizenEventLoop() = default;

bool TizenEventLoop::RunsTasksOnCurrentThread() const {
  return std::this_thread::get_id() == main_thread_id_;
}

gboolean TizenEventLoop::ExecuteExpiredTasksOnMainThread(gpointer data) {
  auto* self = static_cast<TizenEventLoop*>(data);
  self->ExecuteTaskEvents();
  return G_SOURCE_REMOVE;
}

void TizenEventLoop::ExecuteTaskEvents() {
  const TaskTimePoint now = TaskTimePoint::clock::now();
  {
    std::lock_guard<std::mutex> lock1(task_queue_mutex_);
    std::lock_guard<std::mutex> lock2(expired_tasks_mutex_);
    while (!task_queue_.empty()) {
      const Task& top = task_queue_.top();
      if (top.fire_time > now) {
        break;
      }

      expired_tasks_.push_back(task_queue_.top());
      task_queue_.pop();
    }
  }
  OnTaskExpired();
}

void TizenEventLoop::PostTask(FlutterTask flutter_task,
                              uint64_t flutter_target_time_nanos) {
  const uint64_t current_time = get_current_time_();
  const int64_t remaining_nanos =
      static_cast<int64_t>(flutter_target_time_nanos) -
      static_cast<int64_t>(current_time);

  Task task;
  task.order = ++task_order_;
  task.fire_time =
      TaskTimePoint::clock::now() +
      std::chrono::nanoseconds(remaining_nanos > 0 ? remaining_nanos : 0);
  task.task = flutter_task;
  {
    std::lock_guard<std::mutex> lock(task_queue_mutex_);
    task_queue_.push(task);
  }

  GSource* source = nullptr;
  if (remaining_nanos > 0) {
    guint delay_ms =
        static_cast<guint>(std::max<int64_t>(1, remaining_nanos / 1000000));
    source = g_timeout_source_new(delay_ms);
  } else {
    source = g_idle_source_new();
  }

  g_source_set_callback(source, ExecuteExpiredTasksOnMainThread, this, nullptr);
  g_source_attach(source, g_main_context_default());
  g_source_unref(source);
}

TizenPlatformEventLoop::TizenPlatformEventLoop(
    std::thread::id main_thread_id,
    CurrentTimeProc get_current_time,
    TaskExpiredCallback on_task_expired)
    : TizenEventLoop(main_thread_id, get_current_time, on_task_expired) {}

TizenPlatformEventLoop::~TizenPlatformEventLoop() = default;

void TizenPlatformEventLoop::OnTaskExpired() {
  for (const Task& task : expired_tasks_) {
    on_task_expired_(&task.task);
  }
  expired_tasks_.clear();
}

}  // namespace flutter
