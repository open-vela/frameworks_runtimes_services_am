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

struct Label {
    int mId;
    Label(const int Id) : mId(Id){};
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

class TaskMsgHandler {
public:
    TaskMsgHandler(const std::shared_ptr<Task>& task)
          : mTask(task), mIsDone(false), mTimer(nullptr) {}

    void startTimer(UvLoop& loop, uint32_t msTimeout) {
        mTimer = new UvTimer();
        mTimer->init(loop, [this](void*) {
            if (!mIsDone) {
                mIsDone = true;
                mTask->timeout();
            }
        });
        mTimer->start(msTimeout);
    }
    void stopTimer() {
        if (mTimer) {
            mTimer->stop();
            delete mTimer;
            mTimer = nullptr;
        }
    }
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

private:
    std::shared_ptr<Task> mTask;
    bool mIsDone;
    UvTimer* mTimer;
};

class TaskBoard {
public:
    TaskBoard();
    void attachLoop(const std::shared_ptr<UvLoop>& looper);
    void commitTask(const std::shared_ptr<Task>& task, uint32_t msLimitedTime = UINT_MAX);
    void eventTrigger(const Label& e);

private:
    std::list<std::shared_ptr<TaskMsgHandler>> mTasklist;
    std::shared_ptr<UvLoop> mLooper;
};

/**************************** label signature ******************************/
enum TASK_LABEL {
    APP_ATTACH,
    ACTIVITY_STATUS_REPORT,
    ACTIVITY_WAIT_RESUME,
    SERVICE_STATUS_BASE = 210,
    SERVICE_STATUS_END = 230,
};

#define REQUEST_TIMEOUT_MS 10000
/***************************************************************************/

} // namespace am
} // namespace os
