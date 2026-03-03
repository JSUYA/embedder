// Copyright 2020 Samsung Electronics Co., Ltd. All rights reserved.
// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tizen_event_loop.h"

#include <algorithm>
#include <cstdint>
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

  std::lock_guard<std::mutex> lock(wakeup_mutex_);
  if (wakeup_source_id_ != 0) {
    g_source_remove(wakeup_source_id_);
    wakeup_source_id_ = 0;
  }
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

  ScheduleNextWakeup();
}

void TizenEventLoop::ScheduleNextWakeup() {
  if (!is_running_) {
    return;
  }

  TaskTimePoint next_fire_time;
  {
    std::lock_guard<std::mutex> lock(task_queue_mutex_);
    if (task_queue_.empty()) {
      return;
    }
    next_fire_time = task_queue_.top().fire_time;
  }

  std::lock_guard<std::mutex> lock(wakeup_mutex_);

  if (wakeup_source_id_ != 0 && wakeup_fire_time_ <= next_fire_time) {
    // An earlier (or equal) wakeup is already scheduled.
    return;
  }

  if (wakeup_source_id_ != 0) {
    g_source_remove(wakeup_source_id_);
    wakeup_source_id_ = 0;
  }

  const auto now = TaskTimePoint::clock::now();
  const auto delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      next_fire_time - now);
  const guint timeout_msec =
      static_cast<guint>(std::max<int64_t>(0, delta_ms.count()));

  wakeup_fire_time_ = next_fire_time;
  wakeup_source_id_ = g_timeout_add_full(
      G_PRIORITY_DEFAULT, timeout_msec,
      [](gpointer data) -> gboolean {
        auto* self = static_cast<TizenEventLoop*>(data);
        return self->HandleWakeup();
      },
      this, nullptr);
}

gboolean TizenEventLoop::HandleWakeup() {
  {
    std::lock_guard<std::mutex> lock(wakeup_mutex_);
    wakeup_source_id_ = 0;
  }

  if (!is_running_) {
    return G_SOURCE_REMOVE;
  }

  ExecuteTaskEvents();
  ScheduleNextWakeup();
  return G_SOURCE_REMOVE;
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
