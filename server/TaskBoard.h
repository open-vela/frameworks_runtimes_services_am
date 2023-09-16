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
#include <utils/Looper.h>

#include <climits>
#include <functional>
#include <list>
#include <memory>
#include <string>

#include "os/app/IApplicationThread.h"

namespace os {
namespace am {

using android::IBinder;
using android::Looper;
using android::Message;
using android::MessageHandler;
using android::sp;
using os::app::IApplicationThread;
using std::string;

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

class TaskMsgHandler : public MessageHandler {
public:
    TaskMsgHandler(const std::shared_ptr<Task>& task) : mTask(task), mIsDone(false) {}
    void handleMessage(const Message& message) {
        mTask->timeout();
        mIsDone = true;
    }
    Task* getTask() const {
        return mTask.get();
    }
    void doing(const Label& label) {
        mTask->execute(label);
        mIsDone = true;
    }
    bool isDone() const {
        return mIsDone;
    }

private:
    std::shared_ptr<Task> mTask;
    bool mIsDone;
};

class TaskBoard {
public:
    TaskBoard();
    void commitTask(const std::shared_ptr<Task>& task, uint32_t msLimitedTime = UINT_MAX);
    void eventTrigger(const Label& e);

private:
    std::list<sp<TaskMsgHandler>> mTasklist;
    sp<Looper> mLooper;
};

/**************************** label signature ******************************/
enum TASK_LABEL {
    APP_ATTACH,
    ACTIVITY_STATUS_BASE = 100,
    ACTIVITY_STATUS_END = 200,
    SERVICE_STATUS_BASE = 210,
    SERVICE_STATUS_END = 230,
};
/***************************************************************************/

class AppAttachTask : public Task {
public:
    struct Event : Label {
        int mPid;
        int mUid;
        sp<IApplicationThread> mAppHandler;
        Event(const int pid, const int uid, sp<IApplicationThread> app)
              : Label(APP_ATTACH), mPid(pid), mUid(uid), mAppHandler(app) {}
    };

    using TaskFunc = std::function<void(const Event*)>;
    AppAttachTask(const int pid, const TaskFunc& cb) : Task(APP_ATTACH), mPid(pid), mCallback(cb) {}

    bool operator==(const Label& e) const {
        if (mId == e.mId) {
            return mPid == static_cast<const Event*>(&e)->mPid;
        }
        return false;
    }

    void execute(const Label& e) override {
        mCallback(static_cast<const Event*>(&e));
    }

private:
    int mPid;
    TaskFunc mCallback;
};

class ActivityReportStatusTask : public Task {
public:
    struct Event : Label {
        sp<android::IBinder> mToken;
        Event(const int status, const sp<android::IBinder>& token)
              : Label(ACTIVITY_STATUS_BASE + status), mToken(token) {}
    };

    using TaskFunc = std::function<void()>;
    ActivityReportStatusTask(const int status, const sp<android::IBinder>& token,
                             const TaskFunc& cb)
          : Task(ACTIVITY_STATUS_BASE + status), mToken(token), mCallback(cb){};

    bool operator==(const Label& e) const {
        if (mId == e.mId) {
            return mToken == static_cast<const Event*>(&e)->mToken;
        }
        return false;
    }

    void execute(const Label& e) override {
        mCallback();
    }

private:
    sp<android::IBinder> mToken;
    TaskFunc mCallback;
};

class ServiceReportStatusTask : public Task {
public:
    struct Event : Label {
        sp<android::IBinder> mToken;
        Event(const int status, const sp<android::IBinder>& token)
              : Label(SERVICE_STATUS_BASE + status), mToken(token) {}
    };

    using TaskFunc = std::function<void()>;
    ServiceReportStatusTask(const int status, const sp<android::IBinder>& token, const TaskFunc& cb)
          : Task(SERVICE_STATUS_BASE + status), mToken(token), mCallback(cb) {}

    bool operator==(const Label& e) const {
        if (mId == e.mId) {
            return mToken == static_cast<const Event*>(&e)->mToken;
        }
        return false;
    }
    void execute(const Label& e) override {
        mCallback();
    }

private:
    sp<android::IBinder> mToken;
    TaskFunc mCallback;
};

} // namespace am
} // namespace os
