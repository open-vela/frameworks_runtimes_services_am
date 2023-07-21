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
        (mApp.lock()->mAppThread)->scheduleLaunchActivity(mActivityName, mToken, mIntent);
    }
}

void ActivityRecord::start() {
    if (!mApp.expired()) {
        mStatus = STARTING;
        (mApp.lock()->mAppThread)->scheduleStartActivity(mToken, mIntent);
    }
}

void ActivityRecord::resume() {
    if (!mApp.expired()) {
        mStatus = RESUMING;
        mWindowService->updateWindowTokenVisibility(mToken, LayoutParams::WINDOW_VISIBLE);
        (mApp.lock()->mAppThread)->scheduleResumeActivity(mToken, mIntent);
    }
}

void ActivityRecord::pause() {
    if (!mApp.expired()) {
        mStatus = PAUSING;
        mWindowService->updateWindowTokenVisibility(mToken, LayoutParams::WINDOW_INVISIBLE);
        (mApp.lock()->mAppThread)->schedulePauseActivity(mToken);
    }
}

void ActivityRecord::stop() {
    if (!mApp.expired()) {
        mStatus = STOPPING;
        mWindowService->updateWindowTokenVisibility(mToken, LayoutParams::WINDOW_GONE);
        (mApp.lock()->mAppThread)->scheduleStopActivity(mToken);
    }
}

void ActivityRecord::destroy() {
    if (!mApp.expired()) {
        mStatus = DESTROYING;
        (mApp.lock()->mAppThread)->scheduleDestoryActivity(mToken);
    }
}

void ActivityRecord::onResult(int32_t requestCode, int32_t resultCode, const Intent& resultData) {
    if (!mApp.expired()) {
        (mApp.lock()->mAppThread)->onActivityResult(mToken, requestCode, resultCode, resultData);
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
        switch (record.mStatus) {
            case ActivityRecord::CREATING:
                os << "creating";
                break;
            case ActivityRecord::CREATED:
                os << "created";
                break;
            case ActivityRecord::STARTING:
                os << "starting";
                break;
            case ActivityRecord::STARTED:
                os << "started";
                break;
            case ActivityRecord::RESUMING:
                os << "resuming";
                break;
            case ActivityRecord::RESUMED:
                os << "resumed";
                break;
            case ActivityRecord::PAUSING:
                os << "pausing";
                break;
            case ActivityRecord::PAUSED:
                os << "paused";
                break;
            case ActivityRecord::STOPED:
                os << "stoped";
                break;
            case ActivityRecord::STOPPING:
                os << "stopping";
                break;
            case ActivityRecord::DESTROYING:
                os << "destorying";
                break;
            case ActivityRecord::DESTROYED:
                os << "destoryed";
                break;
            default:
                os << "undefined";
                ALOGE("undefined activity status:%d", record.mStatus);
                break;
        }
        os << "] ";
    }
    return os;
}

} // namespace am
} // namespace os
