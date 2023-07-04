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

} // namespace am
} // namespace os
