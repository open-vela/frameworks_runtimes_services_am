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

#include "ActivityStack.h"
#include "AppRecord.h"
#include "TaskBoard.h"

namespace os {
namespace am {

using ActivityStackHandler = std::shared_ptr<ActivityStack>;

/**
 * task list:
 * front-|active|---|foreground task|---|home task|---|background task|-back
 */
class TaskStackManager {
public:
    TaskStackManager(TaskBoard& taskBoard) : mPendTask(taskBoard) {}

    void initHomeTask(const ActivityStackHandler& taskstack, const ActivityHandler& activity);

    /** Launch the app without Activity name, the intent set to the top Activity. */
    void switchTaskToActive(const ActivityStackHandler& task, const Intent& intent);
    /** Move the Task to the background with the activity's order within the task is unchanged */
    bool moveTaskToBackground(const ActivityStackHandler& task);
    /** must be create new Activity */
    void pushNewActivity(const ActivityStackHandler& task, const ActivityHandler& activity,
                         int startFlag);
    /** turn to the Activity which must exist */
    void turnToActivity(const ActivityStackHandler& task, const ActivityHandler& activity,
                        const Intent& intent, int startFlag);
    /** finish a Activity, set result and resume the last */
    void finishActivity(const ActivityHandler& activity);

    /** Lifecycle state management, let Activity goto status */
    void ActivityLifecycleTransition(const ActivityHandler& activity,
                                     ActivityRecord::Status toStatus);

    ActivityHandler getActivity(const sp<IBinder>& token);
    void deleteActivity(const ActivityHandler& activity);

    ActivityStackHandler getActiveTask();
    ActivityStackHandler findTask(const std::string& tag);
    void deleteTask(const ActivityStackHandler& task);
    void pushTaskToFront(const ActivityStackHandler& task);

    friend std::ostream& operator<<(std::ostream& os, const TaskStackManager& task);

private:
    std::list<ActivityStackHandler> mAllTasks;
    ActivityStackHandler mHomeTask;
    std::map<sp<IBinder>, ActivityHandler> mActivityMap;
    TaskBoard& mPendTask;
};

class ActivityLifeCycleTask : public Task {
public:
    struct Event : Label {
        sp<android::IBinder> token;
        ActivityRecord::Status status;
        Event(ActivityRecord::Status s, const sp<android::IBinder>& t)
              : Label(ACTIVITY_STATUS_REPORT), token(t), status(s) {}
    };

    ActivityLifeCycleTask(const ActivityHandler& activity, TaskStackManager* taskManager,
                          ActivityRecord::Status turnTo)
          : Task(ACTIVITY_STATUS_REPORT),
            mActivity(activity),
            mManager(taskManager),
            mTurnTo(turnTo) {}

    bool operator==(const Label& e) const {
        if (mId == e.mId) {
            return mActivity->getToken() == static_cast<const Event*>(&e)->token;
        }
        return false;
    }

    void execute(const Label& e) override {
        const auto event = static_cast<const Event*>(&e);
        if (event->status == ActivityRecord::ERROR) {
            ALOGE("Activity %s[%s] report error!", mActivity->getName().c_str(),
                  mActivity->getStatusStr());
            mActivity->abnormalExit();
            mManager->deleteActivity(mActivity);
            return;
        }

        mActivity->setStatus(event->status);
        if (event->status != ActivityRecord::DESTROYED) {
            mManager->ActivityLifecycleTransition(mActivity, mTurnTo);
        } else {
            mManager->deleteActivity(mActivity);
            if (const auto appRecord = mActivity->getAppRecord()) {
                if (!appRecord->checkActiveStatus()) {
                    appRecord->stopApplication();
                }
            }
        }
    }

    void timeout() override {
        ALOGE("wait Activity %s[%s] reporting timeout!", mActivity->getName().c_str(),
              mActivity->getStatusStr());
        mActivity->abnormalExit();
        mManager->deleteActivity(mActivity);
    }

private:
    ActivityHandler mActivity;
    TaskStackManager* mManager;
    ActivityRecord::Status mTurnTo;
};

class ActivityWaitResume : public Task {
public:
    struct Event : Label {
        sp<android::IBinder> token;
        Event(const sp<android::IBinder>& t) : Label(ACTIVITY_WAIT_RESUME), token(t) {}
    };

    ActivityWaitResume(const ActivityHandler& resumeActivity,
                       const ActivityHandler& willStopActivity, TaskStackManager* manager)
          : Task(ACTIVITY_WAIT_RESUME),
            mResumeActivity(resumeActivity),
            mWillStopActivity(willStopActivity),
            mManager(manager) {}

    void execute(const Label& e) override {
        mManager->ActivityLifecycleTransition(mWillStopActivity, ActivityRecord::STOPPED);
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
        mManager->ActivityLifecycleTransition(mWillStopActivity, ActivityRecord::RESUMED);
    }

private:
    ActivityHandler mResumeActivity;
    ActivityHandler mWillStopActivity;
    TaskStackManager* mManager;
};

} // namespace am
} // namespace os