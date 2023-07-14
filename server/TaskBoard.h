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

#include <functional>
#include <list>
#include <memory>
#include <string>

#include "os/app/IApplicationThread.h"

namespace os {
namespace am {

using android::IBinder;
using android::sp;
using os::app::IApplicationThread;
using std::string;

struct Label {
    int mId;
    Label(const int Id) : mId(Id){};
    bool operator==(const Label& e) {
        return mId == e.mId;
    }
};

class Task : public Label {
public:
    Task(const int Id) : Label(Id) {}
    virtual ~Task(){};
    /**
     * @param Pending event with Label
     * @return: true: Processing completed, event stop
     *          false: Processing completed and continue Passing;
     */
    virtual bool execute(const Label* e) {
        return false;
    }
};

class TaskBoard {
public:
    void commitTask(const std::shared_ptr<Task>& t);
    void eventTrigger(const Label* e);

private:
    std::list<std::shared_ptr<Task>> mTask;
};

/**************************** label signature ******************************/
enum TASK_LABEL {
    APP_ATTACH,
    ACTIVITY_RESUME,
    ACTIVITY_PAUSE,
    SERVICE_DESTROY,
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

    using TaskFunc = std::function<bool(const Event*)>;
    AppAttachTask(const int pid, const TaskFunc& cb) : Task(APP_ATTACH), mPid(pid), mCallback(cb) {}

    bool execute(const Label* e) override {
        const Event* event = static_cast<const Event*>(e);
        if (event->mPid == mPid) {
            return mCallback(event);
        }
        return false;
    }

private:
    int mPid;
    TaskFunc mCallback;
};

class ActivityResumeTask : public Task {
public:
    struct Event : Label {
        sp<android::IBinder> mToken;
        Event(const sp<android::IBinder>& token) : Label(ACTIVITY_RESUME), mToken(token) {}
    };

    using TaskFunc = std::function<bool()>;
    ActivityResumeTask(const sp<android::IBinder>& token, const TaskFunc& cb)
          : Task(ACTIVITY_RESUME), mToken(token), mCallback(cb){};
    bool execute(const Label* e) override {
        const Event* event = static_cast<const Event*>(e);
        if (event->mToken == mToken) {
            return mCallback();
        }
        return false;
    }

private:
    sp<android::IBinder> mToken;
    TaskFunc mCallback;
};

class ActivityPauseTask : public Task {
public:
    struct Event : Label {
        sp<android::IBinder> mToken;
        Event(const sp<android::IBinder>& token) : Label(ACTIVITY_PAUSE), mToken(token) {}
    };

    ActivityPauseTask(const sp<android::IBinder>& token, const std::function<bool()>& cb)
          : Task(ACTIVITY_PAUSE), mToken(token), mCallback(cb){};
    bool execute(const Label* e) override {
        const Event* event = static_cast<const Event*>(e);
        if (event->mToken == mToken) {
            return mCallback();
        }
        return false;
    }

private:
    sp<android::IBinder> mToken;
    std::function<bool()> mCallback;
};

class ServiceDestroyTask : public Task {
public:
    struct Event : Label {
        sp<IBinder> mToken;
        Event(const sp<IBinder>& token) : Label(SERVICE_DESTROY), mToken(token) {}
    };

    using TaskFunc = std::function<bool(const sp<IBinder>&)>;
    ServiceDestroyTask(const sp<IBinder>& token, TaskFunc&& cb)
          : Task(SERVICE_DESTROY), mToken(token), mCallback(cb){};
    bool execute(const Label* e) override {
        const Event* event = static_cast<const Event*>(e);
        if (event->mToken == mToken) {
            return mCallback(mToken);
        }
        return false;
    }

private:
    sp<IBinder> mToken;
    TaskFunc mCallback;
};

} // namespace am
} // namespace os
