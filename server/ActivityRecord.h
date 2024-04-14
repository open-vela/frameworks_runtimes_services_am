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

#include <iostream>
#include <memory>

#include "TaskBoard.h"
#include "app/Intent.h"
#include "os/wm/BnWindowManager.h"

namespace os {
namespace am {

using android::IBinder;
using android::sp;
using os::app::Intent;

class AppRecord;
class ActivityStack;
class TaskStackManager;

using ActivityStackHandler = std::shared_ptr<ActivityStack>;

class ActivityRecord : public std::enable_shared_from_this<ActivityRecord> {
public:
    enum Status {
        ERROR = -1,
        INIT = 0,
        CREATING,
        CREATED,
        STARTING,
        STARTED,
        RESUMING,
        RESUMED,
        PAUSING,
        PAUSED,
        STOPPING,
        STOPPED,
        DESTROYING,
        DESTROYED,
    };

    enum LaunchMode { STANDARD, SINGLE_TOP, SINGLE_TASK, SINGLE_INSTANCE };

    ActivityRecord(const std::string& name, const sp<IBinder>& caller, const int32_t requestCode,
                   const LaunchMode launchMode, const ActivityStackHandler& task,
                   const Intent& intent, sp<::os::wm::IWindowManager> wm, TaskStackManager* tsm,
                   TaskBoard* tb);

    /** Lifecycle state management, let Activity goto status */
    void lifecycleTransition(const Status toStatus);

    void abnormalExit();
    void onResult(int32_t requestCode, int32_t resultCode, const Intent& resultData);

    const sp<IBinder>& getToken() const;
    const std::string& getName() const;
    LaunchMode getLaunchMode() const;

    const sp<IBinder>& getCaller() const;
    int32_t getRequestCode() const;
    ActivityStackHandler getTask() const;

    void setAppThread(const std::shared_ptr<AppRecord>& app);
    std::shared_ptr<AppRecord> getAppRecord() const;

    void setIntent(const Intent& intent);
    const Intent& getIntent() const;

    void setStatus(Status status);
    Status getStatus() const;
    Status getTargetStatus() const;
    void reportError();

    const std::string* getPackageName() const;

    const char* getStatusStr() const;
    static const char* statusToStr(const int status);
    static LaunchMode launchModeToInt(const std::string& launchModeStr);

    friend std::ostream& operator<<(std::ostream& os, const ActivityRecord& record);

private:
    void create();
    void start();
    void resume();
    void pause();
    void stop();
    void destroy();

private:
    std::string mName;
    sp<IBinder> mToken;
    sp<IBinder> mCaller;
    int32_t mRequestCode;
    Status mStatus;
    Status mTargetStatus;
    bool mIsError;
    LaunchMode mLaunchMode;
    std::weak_ptr<AppRecord> mApp;
    std::weak_ptr<ActivityStack> mInTask;
    Intent mIntent;
    bool mNewIntentFlag;

    sp<::os::wm::IWindowManager> mWindowService;
    TaskStackManager* mTaskManager;
    TaskBoard* mPendTask;
};

using ActivityHandler = std::shared_ptr<ActivityRecord>;

class ActivityLifeCycleTask : public Task {
public:
    struct Event : Label {
        sp<android::IBinder> token;
        ActivityRecord::Status status;
        Event(ActivityRecord::Status s, const sp<android::IBinder>& t)
              : Label(ACTIVITY_STATUS_REPORT), token(t), status(s) {}
    };

    ActivityLifeCycleTask(const ActivityHandler& activity, TaskStackManager* taskManager);

    bool operator==(const Label& e) const;
    void execute(const Label& e) override;
    void timeout() override;

private:
    ActivityHandler mActivity;
    TaskStackManager* mTaskManager;
};

class ActivityWaitResume : public Task {
public:
    struct Event : Label {
        sp<android::IBinder> token;
        Event(const sp<android::IBinder>& t) : Label(ACTIVITY_WAIT_RESUME), token(t) {}
    };

    ActivityWaitResume(const ActivityHandler& resumeActivity,
                       const ActivityHandler& willStopActivity)
          : Task(ACTIVITY_WAIT_RESUME),
            mResumeActivity(resumeActivity),
            mWillStopActivity(willStopActivity) {}

    void execute(const Label& e) override {
        if (mResumeActivity->getStatus() >= ActivityRecord::RESUMED &&
            mResumeActivity->getStatus() <= ActivityRecord::STOPPED) {
            mWillStopActivity->lifecycleTransition(ActivityRecord::STOPPED);
        }
    }

    bool operator==(const Label& e) const {
        if (mId == e.mId) {
            return mResumeActivity->getToken() == static_cast<const Event*>(&e)->token;
        }
        return false;
    }

    void timeout() override {
        ALOGE("WaitActivityResume %s[%s] timeout!", mResumeActivity->getName().c_str(),
              mResumeActivity->getStatusStr());
        /** resume the last activity */
        ALOGI("resume %s[%s]", mWillStopActivity->getName().c_str(),
              mWillStopActivity->getStatusStr());
        mWillStopActivity->lifecycleTransition(ActivityRecord::RESUMED);
    }

private:
    ActivityHandler mResumeActivity;
    ActivityHandler mWillStopActivity;
};

class ActivityDelayDestroy : public Task {
public:
    struct Event : Label {
        sp<android::IBinder> token;
        Event(const sp<android::IBinder>& t) : Label(ACTIVITY_DELAY_DESTROY), token(t) {}
    };

    ActivityDelayDestroy(const ActivityHandler& willDestroyActivity,
                         const ActivityHandler& waitResumeActivity)
          : Task(ACTIVITY_DELAY_DESTROY),
            mWillDestroyActivity(willDestroyActivity),
            mWaitResumeActivity(waitResumeActivity) {}

    void execute(const Label& e) override {
        mWillDestroyActivity->lifecycleTransition(ActivityRecord::DESTROYED);
    }

    bool operator==(const Label& e) const {
        if (mId == e.mId) {
            return mWaitResumeActivity->getToken() == static_cast<const Event*>(&e)->token;
        }
        return false;
    }

    void timeout() override {
        ALOGE("WaitActivityResume %s[%s] timeout!", mWaitResumeActivity->getName().c_str(),
              mWaitResumeActivity->getStatusStr());
        /** direct destroy the activity */
        ALOGI("resume %s[%s]", mWillDestroyActivity->getName().c_str(),
              mWillDestroyActivity->getStatusStr());
        mWillDestroyActivity->lifecycleTransition(ActivityRecord::DESTROYED);
    }

private:
    ActivityHandler mWillDestroyActivity;
    ActivityHandler mWaitResumeActivity;
};

} // namespace am
} // namespace os