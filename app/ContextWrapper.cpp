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

#include "app/Context.h"

namespace os {
namespace app {

std::shared_ptr<Context> ContextWrapper::getContext() {
    return mBase;
}

void ContextWrapper::attachBaseContext(std::shared_ptr<Context> base) {
    if (mBase != nullptr) {
        ALOGE("Base context already set");
    }
    mBase = base;
}

std::string ContextWrapper::getPackageName() {
    return mBase->getPackageName();
}

UvLoop* ContextWrapper::getMainLoop() const {
    return mBase->getMainLoop();
}

const android::sp<android::IBinder>& ContextWrapper::getToken() const {
    return mBase->getToken();
}

void ContextWrapper::startActivity(const Intent& intent) {
    return mBase->startActivity(intent);
}

void ContextWrapper::startActivityForResult(const Intent& intent, int requestCode) {
    return mBase->startActivityForResult(intent, requestCode);
}

void ContextWrapper::startService(const Intent& intent) {
    return mBase->startService(intent);
}

void ContextWrapper::setIntent(const Intent& intent) {
    return mBase->setIntent(intent);
}

const Intent& ContextWrapper::getIntent() {
    return mBase->getIntent();
}

} // namespace app
} // namespace os