// Copyright 2020 Samsung Electronics Co., Ltd. All rights reserved.
// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tizen_event_loop.h"

#include <errno.h>
#include <fcntl.h>
#include <glib-unix.h>
#include <unistd.h>

#include <cmath>
#include <utility>

#include "flutter/shell/platform/tizen/logger.h"

namespace flutter {

namespace {

// Writes a single wakeup byte to the pipe. A failed write with EAGAIN means
// the pipe is full, i.e. a wakeup is already pending, so it can be ignored.
void WriteWakeupByte(int fd) {
  if (fd < 0) {
    return;
  }
  const char byte = 1;
  ssize_t bytes_written;
  do {
    bytes_written = write(fd, &byte, sizeof(byte));
  } while (bytes_written < 0 && errno == EINTR);
}

}  // namespace

TizenEventLoop::TizenEventLoop(std::thread::id main_thread_id,
                               CurrentTimeProc get_current_time,
                               TaskExpiredCallback on_task_expired)
    : main_thread_id_(main_thread_id),
      get_current_time_(get_current_time),
      on_task_expired_(std::move(on_task_expired)) {
  if (pipe2(pipe_fds_, O_CLOEXEC | O_NONBLOCK) == 0) {
    pipe_watch_id_ = g_unix_fd_add(
        pipe_fds_[0], G_IO_IN,
        [](gint fd, GIOCondition condition, gpointer data) -> gboolean {
          auto* self = static_cast<TizenEventLoop*>(data);
          // Drain all pending wakeup bytes before processing tasks.
          char buffer[64];
          ssize_t bytes_read;
          do {
            bytes_read = read(fd, buffer, sizeof(buffer));
          } while (bytes_read > 0 || (bytes_read < 0 && errno == EINTR));
          self->ExecuteTaskEvents();
          return G_SOURCE_CONTINUE;
        },
        this);
  } else {
    FT_LOG(Error) << "Failed to create a wakeup pipe for the event loop.";
  }
}

TizenEventLoop::~TizenEventLoop() {
  alive_->store(false);
  if (pipe_watch_id_ > 0) {
    g_source_remove(pipe_watch_id_);
  }
  if (pipe_fds_[0] >= 0) {
    close(pipe_fds_[0]);
  }
  if (pipe_fds_[1] >= 0) {
    close(pipe_fds_[1]);
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

  const double flutter_duration =
      static_cast<double>(flutter_target_time_nanos) - get_current_time_();
  if (flutter_duration > 0) {
    struct TimeoutContext {
      std::shared_ptr<std::atomic<bool>> alive;
      int fd;
    };
    auto* context = new TimeoutContext{alive_, pipe_fds_[1]};
    // GLib timeouts have millisecond resolution. Round up so that the timer
    // never fires before the task's fire time; otherwise the wakeup would find
    // no expired tasks and the task would never be rescheduled.
    g_timeout_add_full(
        G_PRIORITY_DEFAULT,
        static_cast<guint>(std::ceil(flutter_duration / 1000000.0)),
        [](gpointer data) -> gboolean {
          auto* context = static_cast<TimeoutContext*>(data);
          if (context->alive->load()) {
            WriteWakeupByte(context->fd);
          }
          return G_SOURCE_REMOVE;
        },
        context,
        [](gpointer data) { delete static_cast<TimeoutContext*>(data); });
  } else {
    WriteWakeupByte(pipe_fds_[1]);
  }
}

TizenPlatformEventLoop::TizenPlatformEventLoop(
    std::thread::id main_thread_id,
    CurrentTimeProc get_current_time,
    TaskExpiredCallback on_task_expired)
    : TizenEventLoop(main_thread_id, get_current_time, on_task_expired) {}

TizenPlatformEventLoop::~TizenPlatformEventLoop() {}

void TizenPlatformEventLoop::OnTaskExpired() {
  std::vector<Task> local_expired_tasks;
  {
    std::lock_guard<std::mutex> lock(expired_tasks_mutex_);
    local_expired_tasks = std::move(expired_tasks_);
  }

  for (const Task& task : local_expired_tasks) {
    on_task_expired_(&task.task);
  }
}

}  // namespace flutter
