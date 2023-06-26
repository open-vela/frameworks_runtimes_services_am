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

#include "app/ContextImpl.h"

namespace os {
namespace app {

using std::string;

std::shared_ptr<Context> ContextImpl::createActivityContext(Application* app,
                                                            const sp<IBinder>& token) {
    return std::make_shared<ContextImpl>(app, token);
}

ContextImpl::ContextImpl(Application* app, const sp<IBinder>& token) : mApp(app), mToken(token) {}

string ContextImpl::getPackageName() {
    return mApp->getPackageName();
}

void ContextImpl::startActivity(const Intent& intent) {
    mAm.startActivity(mToken, intent, ActivityManager::NO_REQUEST);
}

void ContextImpl::startActivityForResult(const Intent& intent, int32_t requestCode) {
    mAm.startActivity(mToken, intent, requestCode);
}

void ContextImpl::startService(const Intent& intent) {
    mAm.startService(mToken, intent);
}

void ContextImpl::reportActivityStatus(const int32_t status) {
    mAm.reportActivityStatus(mToken, status);
}

void ContextImpl::setIntent(const Intent& intent) {
    mIntent = intent;
}

const Intent& ContextImpl::getIntent() {
    return mIntent;
}

} // namespace app
} // namespace os
