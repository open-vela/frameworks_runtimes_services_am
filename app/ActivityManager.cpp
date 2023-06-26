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

#include "app/ActivityManager.h"

#include <binder/Binder.h>
#include <binder/IServiceManager.h>
#include <utils/String8.h>

namespace os {
namespace app {

using android::binder::Status;
using namespace os::app;

sp<IActivityManager> ActivityManager::getService() {
    std::lock_guard<std::mutex> scoped_lock(mLock);
    if (mService == nullptr || !android::IInterface::asBinder(mService)->isBinderAlive()) {
        if (android::getService<IActivityManager>(android::String16(ActivityManager::name()),
                                                  &mService) != android::NO_ERROR) {
            ALOGE("ServiceManager can't find the service:%s", ActivityManager::name());
        }
    }
    return mService;
}

int ActivityManager::attachApplication(const sp<IApplicationThread>& app) {
    sp<IActivityManager> service = getService();
    int ret = android::FAILED_TRANSACTION;
    if (service != nullptr) {
        Status status = service->attachApplication(app, &ret);
        if (!status.isOk()) {
            ALOGE("attachApplication error:%s", status.toString8().c_str());
        }
    }
    return ret;
}

int ActivityManager::startActivity(const sp<IBinder>& token, const Intent& intent,
                                   int32_t requestCode) {
    sp<IActivityManager> service = getService();
    int ret = android::FAILED_TRANSACTION;
    if (service != nullptr) {
        Status status = service->startActivity(token, intent, requestCode, &ret);
        if (!status.isOk()) {
            ALOGE("startActivity error:%s", status.toString8().c_str());
        }
    }
    return ret;
}

int ActivityManager::finishActivity(const sp<IBinder>& token) {
    sp<IActivityManager> service = getService();
    int ret = android::FAILED_TRANSACTION;
    if (service != nullptr) {
        Status status = service->finishActivity(token, &ret);
        if (!status.isOk()) {
            ALOGE("finishActivity error:%s", status.toString8().c_str());
        }
    }
    return ret;
}

void ActivityManager::returnActivityResult(const sp<IBinder>& token, int32_t resultCode,
                                           const Intent& data) {
    sp<IActivityManager> service = getService();
    if (service != nullptr) {
        Status status = service->returnActivityResult(token, resultCode, data);
        if (!status.isOk()) {
            ALOGE("returnActivityResult error:%s", status.toString8().c_str());
        }
    }
    return;
}

void ActivityManager::reportActivityStatus(const sp<IBinder>& token, int32_t activityStatus) {
    sp<IActivityManager> service = getService();
    if (service != nullptr) {
        Status status = service->reportActivityStatus(token, activityStatus);
        if (!status.isOk()) {
            ALOGE("reportActivityStatus error:%s", status.toString8().c_str());
        }
    }
    return;
}

int ActivityManager::startService(const sp<IBinder>& token, const Intent& intent) {
    sp<IActivityManager> service = getService();
    int ret = android::FAILED_TRANSACTION;
    if (service != nullptr) {
        Status status = service->startService(token, intent, &ret);
        if (!status.isOk()) {
            ALOGE("startService error:%s", status.toString8().c_str());
        }
    }
    return ret;
}

} // namespace app
} // namespace os