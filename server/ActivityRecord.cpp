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
#include "app/Intent.h"
#include "wm/LayoutParams.h"

namespace os {
namespace am {

using os::app::Intent;
using os::wm::LayoutParams;

ActivityRecord::ActivityRecord(const std::string& name, const sp<IBinder>& caller,
                               const int32_t requestCode, const LaunchMode launchMode,
                               const ActivityStackHandler& task, const Intent& intent,
                               sp<::os::wm::IWindowManager> wm) {
    mName = name;
    mToken = new android::BBinder();
    mCaller = caller;
    mRequestCode = requestCode;
    mStatus = CREATING;
    mLaunchMode = launchMode;
    mInTask = task;
    mIntent = intent;
    mWindowService = wm;
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

void ActivityRecord::create() {
    if (mStatus == CREATING) {
        mWindowService->addWindowToken(mToken, LayoutParams::TYPE_APPLICATION, 0);
        const auto appRecord = mApp.lock();
        if (appRecord && appRecord->mIsAlive) {
            ALOGD("scheduleLaunchActivity: %s", mName.c_str());
            appRecord->addActivity(shared_from_this());
            const auto pos = mName.find_first_of('/');
            appRecord->mAppThread->scheduleLaunchActivity(mName.substr(pos + 1, std::string::npos),
                                                          mToken, mIntent);
        }
    }
}

void ActivityRecord::start() {
    if (mStatus > CREATING && mStatus < DESTROYED) {
        mStatus = STARTING;
        const auto appRecord = mApp.lock();
        if (appRecord && appRecord->mIsAlive) {
            ALOGD("scheduleStartActivity: %s", mName.c_str());
            appRecord->mAppThread->scheduleStartActivity(mToken, mIntent);
        }
    }
}

void ActivityRecord::resume() {
    if (mStatus >= STARTING && mStatus <= STOPPED) {
        mStatus = RESUMING;
        const auto appRecord = mApp.lock();
        if (appRecord && appRecord->mIsAlive) {
            ALOGD("scheduleResumeActivity: %s", mName.c_str());
            appRecord->mAppThread->scheduleResumeActivity(mToken, mIntent);
        }
        mWindowService->updateWindowTokenVisibility(mToken, LayoutParams::WINDOW_VISIBLE);
        return;
    }
}

void ActivityRecord::pause() {
    if (mStatus > STARTING && mStatus < PAUSING) {
        mStatus = PAUSING;
        const auto appRecord = mApp.lock();
        if (appRecord && appRecord->mIsAlive) {
            ALOGD("schedulePauseActivity: %s", mName.c_str());
            appRecord->mAppThread->schedulePauseActivity(mToken);
        }
        mWindowService->updateWindowTokenVisibility(mToken, LayoutParams::WINDOW_INVISIBLE);
    }
}

void ActivityRecord::stop() {
    if (mStatus > STARTING && mStatus < STOPPING) {
        mStatus = STOPPING;
        const auto appRecord = mApp.lock();
        if (appRecord && appRecord->mIsAlive) {
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
        if (appRecord && appRecord->mIsAlive) {
            ALOGD("scheduleDestroyActivity: %s", mName.c_str());
            appRecord->deleteActivity(shared_from_this());
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
    }
}

void ActivityRecord::onResult(int32_t requestCode, int32_t resultCode, const Intent& resultData) {
    const auto appRecord = mApp.lock();
    if (appRecord && appRecord->mIsAlive) {
        ALOGD("%s onActivityResult: %d, %d", mName.c_str(), requestCode, resultCode);
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

} // namespace am
} // namespace os
