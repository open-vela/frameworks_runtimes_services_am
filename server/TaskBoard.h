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

#pragma once

#include <binder/IBinder.h>

#include <climits>
#include <list>
#include <memory>

#include "app/UvLoop.h"

namespace os {
namespace am {

using os::app::UvLoop;
using os::app::UvTimer;

enum LabelType {
    ONCE_TRIGGER,
    MULTI_TRIGGER,
};

struct Label {
    const int mId;
    const int mType;
    Label(const int id, int type = ONCE_TRIGGER) : mId(id), mType(type){};
    virtual bool operator==(const Label& e) const {
        return mId == e.mId;
    }
};

class Task : public Label {
public:
    Task(const int Id) : Label(Id) {}
    virtual ~Task(){};
    /**
     * The task is expected to be executed only once,
     * it will either be completed or timeout.
     */
    virtual void execute(const Label& e) = 0;
    virtual void timeout() {
        ALOGW("Task timeouts are not handled in any way!");
    }
};

class TaskTimeoutHandler {
public:
    TaskTimeoutHandler(const std::shared_ptr<Task>& task, const uint64_t usTimeout);

    Task* getTask() const {
        return mTask.get();
    }
    void doing(const Label& label) {
        mIsDone = true;
        mTask->execute(label);
    }
    bool isDone() const {
        return mIsDone;
    }
    uint64_t getExpectTime() const {
        return mExpectTime;
    }
    void timeout() {
        mIsDone = true;
        mTask->timeout();
    }

private:
    std::shared_ptr<Task> mTask;
    bool mIsDone;
    const uint64_t mExpectTime;
};

class TaskBoard {
public:
    TaskBoard();
    void setDebugMode(bool isDebug);
    void startWork(const std::shared_ptr<UvLoop>& looper);
    void commitTask(const std::shared_ptr<Task>& task, const uint64_t msLimitedTime = UINT_MAX);
    void eventTrigger(const Label& e);
    void removeTask(const Label& e);

private:
    void checkTimeout();

    std::list<std::shared_ptr<TaskTimeoutHandler>> mTasklist;
    std::shared_ptr<UvLoop> mLooper;
    uint64_t mNextCheckTime;
    bool mIsDebug;
    UvTimer mTimer;
};

/**************************** label signature ******************************/
enum TASK_LABEL {
    APP_ATTACH,
    ACTIVITY_STATUS_REPORT,
    ACTIVITY_WAIT_RESUME,
    ACTIVITY_DELAY_DESTROY,
    SERVICE_STATUS_BASE = 210,
    SERVICE_STATUS_END = 230,
};

#ifdef CONFIG_MM_KASAN
#define REQUEST_TIMEOUT_MS 30000 // 30 seconds
#else
#define REQUEST_TIMEOUT_MS 10000 // 10 seconds
#endif
/***************************************************************************/

} // namespace am
} // namespace os
