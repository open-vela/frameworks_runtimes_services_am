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

void ActivityRecord::create() {
    if (mStatus == CREATING) {
        mWindowService->addWindowToken(mToken, LayoutParams::TYPE_APPLICATION, 0);
        if (auto appRecord = mApp.lock()) {
            ALOGD("scheduleLaunchActivity: %s/%s", appRecord->mPackageName.c_str(),
                  mActivityName.c_str());
            appRecord->addActivity(shared_from_this());
            appRecord->mAppThread->scheduleLaunchActivity(mActivityName, mToken, mIntent);
        }
    }
}

void ActivityRecord::start() {
    if (mStatus > CREATING && mStatus < DESTROYED) {
        mStatus = STARTING;
        if (auto appRecord = mApp.lock()) {
            ALOGD("scheduleStartActivity: %s/%s", appRecord->mPackageName.c_str(),
                  mActivityName.c_str());
            appRecord->mAppThread->scheduleStartActivity(mToken, mIntent);
        }
    }
}

void ActivityRecord::resume() {
    if (mStatus < STARTING || mStatus > STOPPED) {
        ALOGW("activity:%s want to 'resume' but current is '%s'", mActivityName.c_str(),
              status2Str(mStatus));
        return;
    }
    mStatus = RESUMING;
    if (auto appRecord = mApp.lock()) {
        ALOGD("scheduleResumeActivity: %s/%s", mApp.lock()->mPackageName.c_str(),
              mActivityName.c_str());
        appRecord->mAppThread->scheduleResumeActivity(mToken, mIntent);
    }
    mWindowService->updateWindowTokenVisibility(mToken, LayoutParams::WINDOW_VISIBLE);
}

void ActivityRecord::pause() {
    if (mStatus < PAUSING) {
        mStatus = PAUSING;
        if (auto appRecord = mApp.lock()) {
            ALOGD("schedulePauseActivity: %s/%s", mApp.lock()->mPackageName.c_str(),
                  mActivityName.c_str());
            appRecord->mAppThread->schedulePauseActivity(mToken);
        }
        mWindowService->updateWindowTokenVisibility(mToken, LayoutParams::WINDOW_INVISIBLE);
    }
}

void ActivityRecord::stop() {
    if (mStatus < STOPPING) {
        mStatus = STOPPING;
        if (auto appRecord = mApp.lock()) {
            ALOGD("scheduleStopActivity: %s/%s", mApp.lock()->mPackageName.c_str(),
                  mActivityName.c_str());
            appRecord->mAppThread->scheduleStopActivity(mToken);
        }
        mWindowService->updateWindowTokenVisibility(mToken, LayoutParams::WINDOW_GONE);
    }
}

void ActivityRecord::destroy() {
    if (mStatus < DESTROYING) {
        mStatus = DESTROYING;
        if (auto appRecord = mApp.lock()) {
            ALOGD("scheduleDestroyActivity: %s/%s", mApp.lock()->mPackageName.c_str(),
                  mActivityName.c_str());
            appRecord->deleteActivity(shared_from_this());
            appRecord->mAppThread->scheduleDestroyActivity(mToken);
        }
        mWindowService->removeWindowToken(mToken, 0);
    }
}

void ActivityRecord::abnormalExit() {
    mStatus = DESTROYED;
    if (auto appRecord = mApp.lock()) {
        ALOGW("Activity:%s/%s abnormal exit!", mApp.lock()->mPackageName.c_str(),
              mActivityName.c_str());
        appRecord->deleteActivity(shared_from_this());
        mWindowService->removeWindowToken(mToken, 0);
    }
}

void ActivityRecord::onResult(int32_t requestCode, int32_t resultCode, const Intent& resultData) {
    if (auto appRecord = mApp.lock()) {
        ALOGD("%s/%s onActivityResult: %d, %d", mApp.lock()->mPackageName.c_str(),
              mActivityName.c_str(), requestCode, resultCode);
        appRecord->mAppThread->onActivityResult(mToken, requestCode, resultCode, resultData);
    }
}

const std::string* ActivityRecord::getPackageName() const {
    if (auto appRecord = mApp.lock()) {
        return &(appRecord->mPackageName);
    }
    return nullptr;
}

const char* ActivityRecord::status2Str(const int status) {
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
        default:
            return "undefined";
    }
}

std::ostream& operator<<(std::ostream& os, const ActivityRecord& record) {
    if (auto appRecord = record.mApp.lock()) {
        os << appRecord->mPackageName << "/" << record.mActivityName;
        os << " [";
        os << ActivityRecord::status2Str(record.mStatus);
        os << "] ";
    }
    return os;
}

} // namespace am
} // namespace os
