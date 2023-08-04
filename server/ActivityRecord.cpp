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
    if (!mApp.expired()) {
        mStatus = CREATING;
        mWindowService->addWindowToken(mToken, LayoutParams::TYPE_APPLICATION, 0);
        ALOGD("scheduleLaunchActivity: %s/%s", mApp.lock()->mPackageName.c_str(),
              mActivityName.c_str());
        (mApp.lock()->mAppThread)->scheduleLaunchActivity(mActivityName, mToken, mIntent);
    }
}

void ActivityRecord::start() {
    if (!mApp.expired()) {
        mStatus = STARTING;
        ALOGD("scheduleStartActivity: %s/%s", mApp.lock()->mPackageName.c_str(),
              mActivityName.c_str());
        (mApp.lock()->mAppThread)->scheduleStartActivity(mToken, mIntent);
    }
}

void ActivityRecord::resume() {
    if (!mApp.expired()) {
        mStatus = RESUMING;
        mWindowService->updateWindowTokenVisibility(mToken, LayoutParams::WINDOW_VISIBLE);
        ALOGD("scheduleResumeActivity: %s/%s", mApp.lock()->mPackageName.c_str(),
              mActivityName.c_str());
        (mApp.lock()->mAppThread)->scheduleResumeActivity(mToken, mIntent);
    }
}

void ActivityRecord::pause() {
    if (!mApp.expired()) {
        mStatus = PAUSING;
        mWindowService->updateWindowTokenVisibility(mToken, LayoutParams::WINDOW_INVISIBLE);
        ALOGD("schedulePauseActivity: %s/%s", mApp.lock()->mPackageName.c_str(),
              mActivityName.c_str());
        (mApp.lock()->mAppThread)->schedulePauseActivity(mToken);
    }
}

void ActivityRecord::stop() {
    if (!mApp.expired()) {
        mStatus = STOPPING;
        mWindowService->updateWindowTokenVisibility(mToken, LayoutParams::WINDOW_GONE);
        ALOGD("scheduleStopActivity: %s/%s", mApp.lock()->mPackageName.c_str(),
              mActivityName.c_str());
        (mApp.lock()->mAppThread)->scheduleStopActivity(mToken);
    }
}

void ActivityRecord::destroy() {
    if (!mApp.expired()) {
        mStatus = DESTROYING;
        ALOGD("scheduleDestoryActivity: %s/%s", mApp.lock()->mPackageName.c_str(),
              mActivityName.c_str());
        (mApp.lock()->mAppThread)->scheduleDestoryActivity(mToken);
    }
}

void ActivityRecord::onResult(int32_t requestCode, int32_t resultCode, const Intent& resultData) {
    if (!mApp.expired()) {
        ALOGD("%s/%s onActivityResult: %d, %d", mApp.lock()->mPackageName.c_str(),
              mActivityName.c_str(), requestCode, resultCode);
        (mApp.lock()->mAppThread)->onActivityResult(mToken, requestCode, resultCode, resultData);
    }
}

const std::string* ActivityRecord::getPackageName() const {
    if (!mApp.expired()) {
        return &(mApp.lock()->mPackageName);
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
        case ActivityRecord::STOPED:
            return "stoped";
        case ActivityRecord::DESTROYING:
            return "destroying";
        case ActivityRecord::DESTROYED:
            return "destroyed";
        default:
            return "undefined";
    }
}

sp<::os::wm::IWindowManager>& ActivityRecord::getWindowService() {
    if (mWindowService == nullptr ||
        !android::IInterface::asBinder(mWindowService)->isBinderAlive()) {
        if (android::getService<::os::wm::IWindowManager>(android::String16("window"),
                                                          &mWindowService) != android::NO_ERROR) {
            ALOGE("ServiceManager can't find wms service");
        }
    }
    return mWindowService;
}

std::ostream& operator<<(std::ostream& os, const ActivityRecord& record) {
    if (!record.mApp.expired()) {
        os << record.mApp.lock()->mPackageName << "/" << record.mActivityName;
        os << " [";
        os << ActivityRecord::status2Str(record.mStatus);
        os << "] ";
    }
    return os;
}

} // namespace am
} // namespace os
