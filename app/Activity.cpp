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

#include "app/Activity.h"

#include "BaseWindow.h"
#include "WindowManager.h"
#include "app/ContextImpl.h"

namespace os {
namespace app {

void Activity::setResult(const int resultCode, const std::shared_ptr<Intent>& resultData) {
    mResultCode = resultCode;
    mResultData = resultData;
}

void Activity::finish() {
    getActivityManager().finishActivity(getToken(), mResultCode, mResultData);
}

bool Activity::moveToBackground(bool nonRoot) {
    return getActivityManager().moveActivityTaskToBackground(getToken(), nonRoot);
}

int Activity::attach(std::shared_ptr<Context> context) {
    attachBaseContext(context);

    mWindow = getWindowManager()->newWindow(context.get());
    if (!mWindow) {
        ALOGE("Activity: new window failed!");
        return -1;
    }
    return 0;
}

bool Activity::performCreate() {
    auto wm = getWindowManager();
    if (wm) {
        if (wm->attachIWindow(mWindow) == 0) {
            onCreate();
            return true;
        } else {
            wm->removeWindow(mWindow);
            mWindow.reset();
        }
    }
    return false;
}

bool Activity::performStart() {
    onStart();
    return true;
}

bool Activity::performResume() {
    onResume();
    return true;
}

bool Activity::performPause() {
    onPause();
    return true;
}

bool Activity::performStop() {
    onStop();
    return true;
}

bool Activity::performDestroy() {
    onDestroy();
    auto wm = getWindowManager();
    if (wm && mWindow) {
        wm->removeWindow(mWindow);
        mWindow.reset();
    }
    return true;
}

} // namespace app
} // namespace os
