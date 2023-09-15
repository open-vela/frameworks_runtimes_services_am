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

#include "app/Service.h"

#include "utils/Log.h"

namespace os {
namespace app {

Service::Service() : mServiceBinder(nullptr), mIsBinded(false) {}

sp<IBinder> Service::onBind(const Intent& intent) {
    return nullptr;
}

bool Service::onUnbind() {
    return false;
}

void Service::unbind() {
    if (mIsBinded) {
        onUnbind();
        mIsBinded = false;
    }
}

int Service::bind(const Intent& intent, const sp<IServiceConnection>& conn) {
    if (!mIsBinded) {
        /** bind only once */
        mServiceBinder = onBind(intent);
        getActivityManager().publishService(getToken(), mServiceBinder);
        mIsBinded = true;
    }
    if (mServiceBinder) {
        conn->onServiceConnected(mServiceBinder);
    } else {
        ALOGW("bindService with a null service");
    }

    return 0;
}

} // namespace app
} // namespace os