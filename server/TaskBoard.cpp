/*
 * Copyright (C) 2023 Xiaomi Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "TaskBoard.h"

#include <time.h>

#include <limits>

namespace os {
namespace am {

static uint64_t clock_ms() {
    timespec ts;
    // Use monotonic time.
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ms = ts.tv_sec;
    ms *= 1000;
    ms += ts.tv_nsec / 1000000;
    return ms;
}

#define MIN_CHECKOUT_TIME 1000 // 1 second

TaskTimeoutHandler::TaskTimeoutHandler(const std::shared_ptr<Task>& task, const uint64_t expectTime)
      : mTask(task), mIsDone(false), mExpectTime(expectTime) {}

TaskBoard::TaskBoard() {
    mIsDebug = false;
    mNextCheckTime = ULLONG_MAX;
}

void TaskBoard::setDebugMode(bool isDebug) {
    mIsDebug = isDebug;
}

void TaskBoard::startWork(const std::shared_ptr<UvLoop>& looper) {
    if (!mIsDebug) {
        // start timer
        mTimer.init(looper->get(), [this](void*) { checkTimeout(); });
    } else {
        // This will never start the timer
        mNextCheckTime = 0;
    }
}

void TaskBoard::checkTimeout() {
    const uint64_t now = clock_ms();
    // default. Check for queue task completion in the next one second cycle
    uint64_t nextMinTime = now + MIN_CHECKOUT_TIME;
    for (auto task = mTasklist.begin(); task != mTasklist.end();) {
        const uint64_t expectTime = (*task)->getExpectTime();
        if ((*task)->isDone()) {
            auto tmp = task;
            ++task;
            mTasklist.erase(tmp);
            continue;
        }
        if (now >= expectTime) {
            (*task)->timeout();
        } else if (nextMinTime > expectTime) {
            nextMinTime = expectTime;
        }
        ++task;
    }

    if (!mTasklist.empty()) {
        mTimer.stop();
        mTimer.start(nextMinTime - now);
        mNextCheckTime = nextMinTime;
    } else {
        // non-check if tasklist is empty
        mNextCheckTime = ULLONG_MAX;
    }
    ALOGD("TaskBoard task size:%d, next checkout time:%lld", mTasklist.size(), mNextCheckTime);
}

void TaskBoard::commitTask(const std::shared_ptr<Task>& task, const uint64_t msLimitedTime) {
    const uint64_t now = clock_ms();
    const auto taskHandler = std::make_shared<TaskTimeoutHandler>(task, now + msLimitedTime);
    if (mNextCheckTime > taskHandler->getExpectTime()) {
        mNextCheckTime = taskHandler->getExpectTime();
        const uint64_t timeout = mNextCheckTime - now;
        mTimer.stop();
        mTimer.start(timeout < MIN_CHECKOUT_TIME ? timeout : MIN_CHECKOUT_TIME);
    }
    mTasklist.emplace_back(taskHandler);
}

void TaskBoard::eventTrigger(const Label& e) {
    for (auto taskIter = mTasklist.begin(); taskIter != mTasklist.end(); ++taskIter) {
        if (!(*taskIter)->isDone() && (*(*taskIter)->getTask() == e)) {
            (*taskIter)->doing(e);
            if (e.mType == LabelType::ONCE_TRIGGER) {
                break;
            }
        }
    }
}

} // namespace am
} // namespace os