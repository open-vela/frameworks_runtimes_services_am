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
    ALOGD("reportActivityStatus: %s[%p] status:%d", mActivityName.c_str(),
          mActivity->getToken().get(), status);
    mStatus = status;
    mActivity->getActivityManager().reportActivityStatus(mActivity->getToken(), status);
}

void ActivityClientRecord::onActivityResult(const int requestCode, const int resultCode,
                                            const Intent& resultData) {
    mActivity->onActivityResult(requestCode, resultCode, resultData);
}

void ActivityClientRecord::onCreate(const Intent& intent) {
    ALOGD("Activity onCreate: %s[%p]", mActivityName.c_str(), mActivity->getToken().get());
    mActivity->setIntent(intent);
    mActivity->performCreate();
    reportActivityStatus(CREATED);
}

void ActivityClientRecord::onStart(const Intent& intent) {
    ALOGD("Activity onStart: %s[%p]", mActivityName.c_str(), mActivity->getToken().get());
    if (mStatus == STOPPED) {
        mActivity->setIntent(intent);
        ALOGD("Activity onNewIntent: %s[%p]", mActivityName.c_str(), mActivity->getToken().get());
        mActivity->onNewIntent(intent);
    }

    mActivity->performStart();
    reportActivityStatus(STARTED);
}

void ActivityClientRecord::onResume(const Intent& intent) {
    ALOGD("Activity onResume: %s[%p]", mActivityName.c_str(), mActivity->getToken().get());
    if (mStatus == PAUSED) {
        mActivity->setIntent(intent);
        mActivity->onNewIntent(intent);
    }

    mActivity->performResume();
    reportActivityStatus(RESUMED);
}

void ActivityClientRecord::onPause() {
    ALOGD("Activity onPause: %s[%p]", mActivityName.c_str(), mActivity->getToken().get());
    mActivity->performPause();
    reportActivityStatus(PAUSED);
}

void ActivityClientRecord::onStop() {
    ALOGD("Activity onStop: %s[%p]", mActivityName.c_str(), mActivity->getToken().get());
    mActivity->performStop();
    reportActivityStatus(STOPPED);
}

void ActivityClientRecord::onDestroy() {
    ALOGD("Activity onDestroy: %s[%p]", mActivityName.c_str(), mActivity->getToken().get());
    mActivity->performDestroy();
    reportActivityStatus(DESTROYED);
}

} // namespace app
} // namespace os