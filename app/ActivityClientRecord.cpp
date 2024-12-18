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

#include "ActivityClientRecord.h"

namespace os {
namespace app {

ActivityClientRecord::ActivityClientRecord(const string& name,
                                           const std::shared_ptr<Activity> activity)
      : mActivityName(name), mActivity(activity), mStatus(CREATING) {}

ActivityClientRecord::~ActivityClientRecord() {}

void ActivityClientRecord::reportActivityStatus(const int32_t status) {
    ALOGD("reportActivityStatus: %s[%p] status:%" PRId32 "", mActivityName.c_str(),
          mActivity->getToken().get(), status);
    mStatus = status;
    mActivity->getActivityManager().reportActivityStatus(mActivity->getToken(), status);
}

int32_t ActivityClientRecord::getStatus() {
    return mStatus;
}

void ActivityClientRecord::onActivityResult(const int requestCode, const int resultCode,
                                            const Intent& resultData) {
    mActivity->onActivityResult(requestCode, resultCode, resultData);
}

int ActivityClientRecord::onCreate(const Intent& intent) {
    ALOGD("Activity onCreate: %s[%p]", mActivityName.c_str(), mActivity->getToken().get());
    mActivity->setIntent(intent);
    if (mActivity->performCreate()) {
        reportActivityStatus(CREATED);
    } else {
        reportActivityStatus(ERROR);
        return -1;
    }
    return 0;
}

int ActivityClientRecord::onStart(const std::optional<Intent>& intent) {
    ALOGD("Activity onStart: %s[%p]", mActivityName.c_str(), mActivity->getToken().get());
    if (mStatus == CREATED) {
        if (intent.has_value()) {
            mActivity->setIntent(intent.value());
        }
    } else {
        if (intent.has_value()) {
            ALOGD("Activity onNewIntent: %s[%p]", mActivityName.c_str(),
                  mActivity->getToken().get());
            mActivity->setIntent(intent.value());
            mActivity->onNewIntent(intent.value());
        }
        if (mStatus == STOPPED) {
            mActivity->onRestart();
        }
    }

    mActivity->performStart();
    reportActivityStatus(STARTED);
    return 0;
}

int ActivityClientRecord::onResume(const std::optional<Intent>& intent) {
    ALOGD("Activity onResume: %s[%p]", mActivityName.c_str(), mActivity->getToken().get());
    if (intent.has_value()) {
        ALOGD("Activity onNewIntent: %s[%p]", mActivityName.c_str(), mActivity->getToken().get());
        mActivity->setIntent(intent.value());
        mActivity->onNewIntent(intent.value());
    }

    mActivity->performResume();
    reportActivityStatus(RESUMED);
    return 0;
}

int ActivityClientRecord::onPause() {
    ALOGD("Activity onPause: %s[%p]", mActivityName.c_str(), mActivity->getToken().get());
    mActivity->performPause();
    reportActivityStatus(PAUSED);
    return 0;
}

int ActivityClientRecord::onStop() {
    ALOGD("Activity onStop: %s[%p]", mActivityName.c_str(), mActivity->getToken().get());
    mActivity->performStop();
    reportActivityStatus(STOPPED);
    return 0;
}

int ActivityClientRecord::onDestroy() {
    ALOGD("Activity onDestroy: %s[%p]", mActivityName.c_str(), mActivity->getToken().get());
    mActivity->performDestroy();
    reportActivityStatus(DESTROYED);
    return 0;
}

void ActivityClientRecord::handleReceiveIntent(const Intent& intent) {
    ALOGD("Activity handleReceiveIntent:%s, action:%s", mActivityName.c_str(),
          intent.mAction.c_str());
    if (intent.mAction == Intent::ACTION_BACK_PRESSED) {
        mActivity->onBackPressed();
    } else {
        mActivity->onReceiveIntent(intent);
    }
}

} // namespace app
} // namespace os