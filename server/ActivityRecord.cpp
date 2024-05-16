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

#define LOG_TAG "AMS"

#include "ActivityRecord.h"

#include <binder/IInterface.h>
#include <binder/IServiceManager.h>
#include <utils/Log.h>

#include "AppRecord.h"
#include "TaskStackManager.h"
#include "app/Intent.h"
#include "wm/LayoutParams.h"

namespace os {
namespace am {

using os::app::Intent;
using os::wm::LayoutParams;

ActivityRecord::ActivityRecord(const std::string& name, const sp<IBinder>& caller,
                               const int32_t requestCode, const LaunchMode launchMode,
                               const ActivityStackHandler& task, const Intent& intent,
                               sp<::os::wm::IWindowManager> wm, TaskStackManager* tsm,
                               TaskBoard* tb) {
    mName = name;
    mToken = new android::BBinder();
    mCaller = caller;
    mRequestCode = requestCode;
    mStatus = INIT;
    mTargetStatus = INIT;
    mIsError = false;
    mLaunchMode = launchMode;
    mInTask = task;
    mIntent = intent;
    mWindowService = wm;
    mTaskManager = tsm;
    mPendTask = tb;
    mNewIntentFlag = true;
}

const sp<IBinder>& ActivityRecord::getToken() const {
    return mToken;
}

const std::string& ActivityRecord::getName() const {
    return mName;
}

ActivityRecord::LaunchMode ActivityRecord::getLaunchMode() const {
    return mLaunchMode;
}

const sp<IBinder>& ActivityRecord::getCaller() const {
    return mCaller;
}

int32_t ActivityRecord::getRequestCode() const {
    return mRequestCode;
}

ActivityStackHandler ActivityRecord::getTask() const {
    return mInTask.lock();
}

void ActivityRecord::setAppThread(const std::shared_ptr<AppRecord>& app) {
    mApp = app;
}

std::shared_ptr<AppRecord> ActivityRecord::getAppRecord() const {
    return mApp.lock();
}

void ActivityRecord::setIntent(const Intent& intent) {
    mIntent = intent;
    mNewIntentFlag = true;
}

const Intent& ActivityRecord::getIntent() const {
    return mIntent;
}

void ActivityRecord::setStatus(Status status) {
    mStatus = status;
}

ActivityRecord::Status ActivityRecord::getStatus() const {
    return mStatus;
}

ActivityRecord::Status ActivityRecord::getTargetStatus() const {
    return mTargetStatus;
}

void ActivityRecord::reportError() {
    mIsError = true;
    mStatus = (Status)(mStatus - 1);
}

void ActivityRecord::lifecycleTransition(const Status toStatus) {
    enum { NONE = 0, CREATE, START, RESUME, PAUSE, STOP, DESTROY };
    static int lifeCycleTable[7][7] = {
            /*      →  X :toStatus, targetStatus                        */
            /* ↓ Y :mStatus, curStatus                                  */
            /*      none, create, start, resume, pause, stop, destroy   */
            /*init*/ {NONE, CREATE, CREATE, CREATE, CREATE, CREATE, NONE},
            /*create*/ {NONE, NONE, START, START, START, STOP, DESTROY},
            /*start*/ {NONE, NONE, NONE, RESUME, PAUSE, STOP, STOP},
            /*resume*/ {NONE, NONE, START, NONE, PAUSE, PAUSE, PAUSE},
            /*pause*/ {NONE, NONE, START, RESUME, NONE, STOP, STOP},
            /*stop*/ {NONE, NONE, START, START, NONE, NONE, DESTROY},
            /*destroy*/ {NONE, NONE, NONE, NONE, NONE, NONE, NONE},
    };

    if (mTargetStatus == RESUMED && toStatus > RESUMED && mStatus < RESUMING) {
        // Other Activity wait for this Activity "resume", but it can't to resume.
        // then we trigger the "ActivityWaitResume" task.
        const ActivityWaitResume::Event event(mToken);
        mPendTask->eventTrigger(event);
    }

    if (mStatus % 2 == 1) {
        // when Activity status is:creating, starting, resuming, **ing.
        // we can't do anything except wait for it to report
        mTargetStatus = toStatus;
        return;
    }

    const int turnTo = lifeCycleTable[(mStatus + 1) >> 1][(toStatus + 1) >> 1];
    switch (turnTo) {
        case CREATE:
            create();
            break;
        case START:
            start();
            break;
        case RESUME:
            resume();
            break;
        case PAUSE:
            pause();
            break;
        case STOP:
            stop();
            break;
        case DESTROY:
            destroy();
            break;
        case NONE:
            ALOGD("lifecycleTransition %s[%s] done", mName.c_str(), getStatusStr());
            return;
    }
    mTargetStatus = toStatus;
    ALOGI("lifecycleTransition %s [%s] to [%s]", mName.c_str(), getStatusStr(),
          statusToStr(toStatus));
    const auto task = std::make_shared<ActivityLifeCycleTask>(shared_from_this(), mTaskManager);
    mPendTask->commitTask(task, REQUEST_TIMEOUT_MS);
}

void ActivityRecord::create() {
    if (mStatus == INIT) {
        mStatus = CREATING;
        mWindowService->addWindowToken(mToken, LayoutParams::TYPE_APPLICATION, 0);
        const auto appRecord = mApp.lock();
        if (appRecord && appRecord->mStatus != APP_STOPPED) {
            ALOGD("scheduleLaunchActivity: %s", mName.c_str());
            appRecord->addActivity(shared_from_this());
            const auto pos = mName.find_first_of('/');
            appRecord->mAppThread->scheduleLaunchActivity(mName.substr(pos + 1, std::string::npos),
                                                          mToken, mIntent);
            mNewIntentFlag = false;
        }
    }
}

void ActivityRecord::start() {
    if (mStatus > CREATING && mStatus < DESTROYED) {
        mStatus = STARTING;
        const auto appRecord = mApp.lock();
        if (appRecord && appRecord->mStatus != APP_STOPPED) {
            ALOGD("scheduleStartActivity: %s", mName.c_str());
            const std::optional<Intent> intent = mNewIntentFlag
                    ? std::optional<std::reference_wrapper<Intent>>(mIntent)
                    : std::nullopt;
            appRecord->mAppThread->scheduleStartActivity(mToken, intent);
            mNewIntentFlag = false;
        }
    }
}

void ActivityRecord::resume() {
    if (mStatus >= STARTING && mStatus <= STOPPED) {
        mStatus = RESUMING;
        const auto appRecord = mApp.lock();
        if (appRecord && appRecord->mStatus != APP_STOPPED) {
            ALOGD("scheduleResumeActivity: %s", mName.c_str());
            const std::optional<Intent> intent = mNewIntentFlag
                    ? std::optional<std::reference_wrapper<Intent>>(mIntent)
                    : std::nullopt;
            appRecord->mAppThread->scheduleResumeActivity(mToken, intent);
            mNewIntentFlag = false;
        }
        mWindowService->updateWindowTokenVisibility(mToken, LayoutParams::WINDOW_VISIBLE);
        return;
    }
}

void ActivityRecord::pause() {
    if (mStatus > STARTING && mStatus < PAUSING) {
        mStatus = PAUSING;
        const auto appRecord = mApp.lock();
        if (appRecord && appRecord->mStatus != APP_STOPPED) {
            ALOGD("schedulePauseActivity: %s", mName.c_str());
            appRecord->mAppThread->schedulePauseActivity(mToken);
        }
        mWindowService->updateWindowTokenVisibility(mToken, LayoutParams::WINDOW_INVISIBLE);
    }
}

void ActivityRecord::stop() {
    if (mStatus > CREATING && mStatus < STOPPING) {
        mStatus = STOPPING;
        const auto appRecord = mApp.lock();
        if (appRecord && appRecord->mStatus != APP_STOPPED) {
            ALOGD("scheduleStopActivity: %s", mName.c_str());
            appRecord->mAppThread->scheduleStopActivity(mToken);
        }
        mWindowService->updateWindowTokenVisibility(mToken, LayoutParams::WINDOW_GONE);
    }
}

void ActivityRecord::destroy() {
    if (mStatus > CREATING && mStatus < DESTROYING) {
        mStatus = DESTROYING;
        const auto appRecord = mApp.lock();
        if (appRecord && appRecord->mStatus != APP_STOPPED) {
            ALOGD("scheduleDestroyActivity: %s", mName.c_str());
            appRecord->mAppThread->scheduleDestroyActivity(mToken);
        }
        mWindowService->removeWindowToken(mToken, 0);
    }
}

void ActivityRecord::abnormalExit() {
    mStatus = DESTROYED;
    if (auto appRecord = mApp.lock()) {
        ALOGW("Activity:%s abnormal exit!", mName.c_str());
        appRecord->deleteActivity(shared_from_this());
        mWindowService->removeWindowToken(mToken, 0);
        appRecord->stopApplication();
    }
}

void ActivityRecord::onResult(int32_t requestCode, int32_t resultCode, const Intent& resultData) {
    const auto appRecord = mApp.lock();
    if (appRecord && appRecord->mStatus != APP_STOPPED) {
        ALOGD("%s onActivityResult: %" PRId32 " %" PRId32 "", mName.c_str(), requestCode,
              resultCode);
        appRecord->mAppThread->onActivityResult(mToken, requestCode, resultCode, resultData);
    }
}

const std::string* ActivityRecord::getPackageName() const {
    if (auto appRecord = mApp.lock()) {
        return &(appRecord->mPackageName);
    }
    return nullptr;
}

ActivityRecord::LaunchMode ActivityRecord::launchModeToInt(const std::string& launchMode) {
    if (launchMode == "standard") {
        return ActivityRecord::STANDARD;
    } else if (launchMode == "singleTask") {
        return ActivityRecord::SINGLE_TASK;
    } else if (launchMode == "singleTop") {
        return ActivityRecord::SINGLE_TOP;
    } else if (launchMode == "singleInstance") {
        return ActivityRecord::SINGLE_INSTANCE;
    } else {
        ALOGW("Activity launchMode:%s is illegally", launchMode.c_str());
        return ActivityRecord::STANDARD;
    }
}

const char* ActivityRecord::getStatusStr() const {
    return statusToStr(mStatus);
}

const char* ActivityRecord::statusToStr(const int status) {
    switch (status) {
        case ActivityRecord::INIT:
            return "init";
        case ActivityRecord::CREATING:
            return "creating";
        case ActivityRecord::CREATED:
            return "created";
        case ActivityRecord::STARTING:
            return "starting";
        case ActivityRecord::STARTED:
            return "started";
        case ActivityRecord::RESUMING:
            return "resuming";
        case ActivityRecord::RESUMED:
            return "resumed";
        case ActivityRecord::PAUSING:
            return "pausing";
        case ActivityRecord::PAUSED:
            return "paused";
        case ActivityRecord::STOPPING:
            return "stopping";
        case ActivityRecord::STOPPED:
            return "stoped";
        case ActivityRecord::DESTROYING:
            return "destroying";
        case ActivityRecord::DESTROYED:
            return "destroyed";
        case ActivityRecord::ERROR:
            return "error";
        default:
            return "undefined";
    }
}

std::ostream& operator<<(std::ostream& os, const ActivityRecord& record) {
    os << record.mName;
    os << " [";
    os << ActivityRecord::statusToStr(record.mStatus);
    os << "] ";
    return os;
}

ActivityLifeCycleTask::ActivityLifeCycleTask(const ActivityHandler& activity,
                                             TaskStackManager* taskManager)
      : Task(ACTIVITY_STATUS_REPORT), mActivity(activity), mTaskManager(taskManager) {}

bool ActivityLifeCycleTask::operator==(const Label& e) const {
    if (mId == e.mId) {
        return mActivity->getToken() == static_cast<const Event*>(&e)->token;
    }
    return false;
}

void ActivityLifeCycleTask::execute(const Label& e) {
    const auto event = static_cast<const Event*>(&e);
    if (event->status == ActivityRecord::ERROR) {
        ALOGE("Activity %s[%s] report error!", mActivity->getName().c_str(),
              mActivity->getStatusStr());
        mActivity->reportError();
        mTaskManager->deleteActivity(mActivity);
    } else {
        mActivity->setStatus(event->status);
        mActivity->lifecycleTransition(mActivity->getTargetStatus());
    }
}

void ActivityLifeCycleTask::timeout() {
    if (mActivity->getStatus() == mActivity->getTargetStatus()) {
        ALOGI("finish transport lifecycle:%s", mActivity->getStatusStr());
        return;
    }

    ALOGE("wait Activity %s[%s] reporting timeout!", mActivity->getName().c_str(),
          mActivity->getStatusStr());
    mActivity->abnormalExit();
    mTaskManager->deleteActivity(mActivity);
}

} // namespace am
} // namespace os
