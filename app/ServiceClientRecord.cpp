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

#include "ServiceClientRecord.h"

namespace os {
namespace app {

ServiceClientRecord::ServiceClientRecord(const string& name,
                                         const std::shared_ptr<Service>& service)
      : mServiceName(name), mService(service), mStatus(CREATING), mStartFlag(F_UNKNOW) {}

ServiceClientRecord::~ServiceClientRecord() {}

void ServiceClientRecord::reportServiceStatus(const int32_t status) {
    ALOGD("reportServiceStatus: %s[%p] status:%" PRId32 "", mServiceName.c_str(),
          mService->getToken().get(), status);
    mStatus = status;
    mService->getActivityManager().reportServiceStatus(mService->getToken(), status);
}

const sp<IBinder>& ServiceClientRecord::getToken() {
    return mService->getToken();
}

int32_t ServiceClientRecord::getStatus() {
    return mStatus;
}

void ServiceClientRecord::onStart(const Intent& intent) {
    if (mStatus == CREATING) {
        ALOGD("Service onCreate: %s[%p]", mServiceName.c_str(), mService->getToken().get());
        mService->onCreate();
        reportServiceStatus(CREATED);
    }
    ALOGD("Service onStart: %s[%p]", mServiceName.c_str(), mService->getToken().get());
    mService->setIntent(intent);
    mService->onStartCommand(intent);
    mStartFlag |= F_STARTED;
    reportServiceStatus(STARTED);
}

void ServiceClientRecord::onBind(const Intent& intent, const sp<IServiceConnection>& conn) {
    if (mStatus == CREATING) {
        ALOGD("Service onCreate: %s[%p]", mServiceName.c_str(), mService->getToken().get());
        mService->onCreate();
        reportServiceStatus(CREATED);
    }
    ALOGD("Service onBind: %s[%p]", mServiceName.c_str(), mService->getToken().get());
    mService->setIntent(intent);
    mService->bind(intent, conn);
    mStartFlag |= F_BINDED;
    reportServiceStatus(BINDED);
}

void ServiceClientRecord::onUnbind() {
    if (mStartFlag & F_BINDED) {
        ALOGD("Service onUnbind: %s[%p]", mServiceName.c_str(), mService->getToken().get());
        mService->unbind();
        reportServiceStatus(UNBINDED);
    }
}

void ServiceClientRecord::onDestroy() {
    ALOGD("Service onDestroy: %s[%p]", mServiceName.c_str(), mService->getToken().get());
    mService->onDestroy();
    reportServiceStatus(DESTROYED);
}

void ServiceClientRecord::handleReceiveIntent(const Intent& intent) {
    ALOGD("Service onReceiveIntent: %s[%p]", mServiceName.c_str(), mService->getToken().get());
    return mService->onReceiveIntent(intent);
}

} // namespace app
} // namespace os