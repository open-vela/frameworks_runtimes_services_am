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

#include <utils/Log.h>

#include "AppRecord.h"
#include "app/Intent.h"

namespace os {
namespace am {

using os::app::Intent;

void ActivityRecord::resume() {
    if (!mApp.expired()) {
        mStatus = RESUMING;
        (mApp.lock()->mAppThread)->scheduleResumeActivity(mToken, mIntent);
    }
}

void ActivityRecord::pause() {
    if (!mApp.expired()) {
        mStatus = PAUSING;
        (mApp.lock()->mAppThread)->schedulePauseActivity(mToken);
    }
}

void ActivityRecord::stop() {
    if (!mApp.expired()) {
        mStatus = STOPPING;
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
