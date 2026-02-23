// Copyright 2020 Samsung Electronics Co., Ltd. All rights reserved.
// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tizen_event_loop.h"

#include <algorithm>
#include <glib.h>

#include <utility>

namespace flutter {

TizenEventLoop::TizenEventLoop(std::thread::id main_thread_id,
                               CurrentTimeProc get_current_time,
                               TaskExpiredCallback on_task_expired)
    : main_thread_id_(main_thread_id),
      get_current_time_(get_current_time),
      on_task_expired_(std::move(on_task_expired)) {}

TizenEventLoop::~TizenEventLoop() {
  is_running_ = false;
}

bool TizenEventLoop::RunsTasksOnCurrentThread() const {
  return std::this_thread::get_id() == main_thread_id_;
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

TizenEventLoop::TaskTimePoint TizenEventLoop::TimePointFromFlutterTime(
    uint64_t flutter_target_time_nanos) {
  const TaskTimePoint now = TaskTimePoint::clock::now();
  const uint64_t flutter_duration =
      flutter_target_time_nanos - get_current_time_();
  return now + std::chrono::nanoseconds(flutter_duration);
}

void TizenEventLoop::PostTask(FlutterTask flutter_task,
                              uint64_t flutter_target_time_nanos) {
  Task task;
  task.order = ++task_order_;
  task.fire_time = TimePointFromFlutterTime(flutter_target_time_nanos);
  task.task = flutter_task;
  {
    std::lock_guard<std::mutex> lock(task_queue_mutex_);
    task_queue_.push(task);
  }

  const double flutter_duration =
      static_cast<double>(flutter_target_time_nanos) - get_current_time_();
  if (flutter_duration > 0) {
    const guint timeout_msec = static_cast<guint>(
        std::max(1.0, flutter_duration / 1000000.0 /* nanos -> millis */));
    g_timeout_add_full(
        G_PRIORITY_DEFAULT, timeout_msec,
        [](gpointer data) -> gboolean {
          auto* self = static_cast<TizenEventLoop*>(data);
          if (self->is_running_) {
            self->ExecuteTaskEvents();
          }
          return G_SOURCE_REMOVE;
        },
        this, nullptr);
  } else {
    g_main_context_invoke(nullptr,  // default context
                          [](gpointer data) -> gboolean {
                            auto* self = static_cast<TizenEventLoop*>(data);
                            if (self->is_running_) {
                              self->ExecuteTaskEvents();
                            }
                            return G_SOURCE_REMOVE;
                          },
                          this);
  }
}

TizenPlatformEventLoop::TizenPlatformEventLoop(
    std::thread::id main_thread_id,
    CurrentTimeProc get_current_time,
    TaskExpiredCallback on_task_expired)
    : TizenEventLoop(main_thread_id, get_current_time, on_task_expired) {}

TizenPlatformEventLoop::~TizenPlatformEventLoop() {}

void TizenPlatformEventLoop::OnTaskExpired() {
  for (const Task& task : expired_tasks_) {
    on_task_expired_(&task.task);
  }
  expired_tasks_.clear();
}

}  // namespace flutter
