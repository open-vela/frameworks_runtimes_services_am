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

#define CONTEXT_IMPL static_cast<ContextImpl*>(mBase.get())

void Activity::reportActivityStatus(const int status) {
    mStatus = status;
    ALOGD("reportActivityStatus: token[%p] status:%d", CONTEXT_IMPL->mToken.get(), status);
    CONTEXT_IMPL->mAm.reportActivityStatus(CONTEXT_IMPL->mToken, status);
}

void Activity::setResult(const int resultCode, const std::shared_ptr<Intent>& resultData) {
    mResultCode = resultCode;
    mResultData = resultData;
}

void Activity::finish() {
    CONTEXT_IMPL->mAm.finishActivity(CONTEXT_IMPL->mToken, mResultCode, mResultData);
}

int Activity::getStatus() {
    return mStatus;
}

void Activity::attach(std::shared_ptr<Context> context, Intent intent) {
    attachBaseContext(context);
    setIntent(intent);

    mWindow = ::os::wm::WindowManager::getInstance()->newWindow(context.get());
    if (mWindow) {
        mWindowManager = (::os::wm::WindowManager*)mWindow->getWindowManager();

    } else {
        ALOGE("Activity: new window failed!");
    }
}

void Activity::performCreate() {
    if (mWindowManager) mWindowManager->attachIWindow(mWindow);
    onCreate();
}

void Activity::performStart() {
    onStart();
}

void Activity::performResume() {
    onResume();
}

void Activity::performPause() {
    onPause();
}

void Activity::performStop() {
    onStop();
    if (mWindowManager && mWindow) mWindowManager->removeWindow(mWindow);
    mWindow.reset();
}

void Activity::performDestroy() {
    if (mWindowManager && mWindow) mWindowManager->removeWindow(mWindow);
    onDestory();
}

} // namespace app
} // namespace os