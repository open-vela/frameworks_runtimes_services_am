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

#include "ServiceRecord.h"

#include <binder/IInterface.h>

#include <optional>

#include "AppRecord.h"

namespace os {
namespace am {

using std::string;

void ServiceRecord::start(const Intent& intent) {
    if (!mApp.expired()) {
        mStartFlag |= F_STARTED;
        (mApp.lock()->mAppThread)->scheduleStartService(mServiceName, mToken, intent);
    }
}

void ServiceRecord::stop() {
    if (!mApp.expired()) {
        for (auto iter : mConnectRecord) {
            iter->onServiceDisconnected(mServiceBinder);
        }
        (mApp.lock()->mAppThread)->scheduleStopService(mToken);
    }
}

void ServiceRecord::bind(const sp<IBinder>& caller, const sp<IServiceConnection>& conn,
                         const Intent& intent) {
    if (!mApp.expired()) {
        mStartFlag |= F_BINDED;
        if (mServiceBinder) {
            conn->onServiceConnected(mServiceBinder);
        } else {
            (mApp.lock()->mAppThread)->scheduleBindService(mServiceName, mToken, intent, conn);
        }
        bool isExist = false;
        for (auto iter : mConnectRecord) {
            if (android::IInterface::asBinder(iter) == android::IInterface::asBinder(conn)) {
                isExist = true;
                break;
            }
        }
        if (!isExist) mConnectRecord.emplace_back(conn);
    }
}

void ServiceRecord::unbind(const sp<IServiceConnection>& conn) {
    if (mStartFlag & F_BINDED) {
        const int size = mConnectRecord.size();
        for (int i = 0; i < size; ++i) {
            if (android::IInterface::asBinder(mConnectRecord[i]) ==
                android::IInterface::asBinder(conn)) {
                mConnectRecord[i] = mConnectRecord[size - 1];
                mConnectRecord.pop_back();
                break;
            }
        }
        if (!mApp.expired()) {
            if (mConnectRecord.empty()) {
                mStartFlag &= ~F_BINDED;
                (mApp.lock()->mAppThread)->scheduleUnbindService(mToken);
            }
        }
    }
}

bool ServiceRecord::isAlive() {
    return mStartFlag != F_UNKNOW;
}

const string* ServiceRecord::getPackageName() {
    if (!mApp.expired()) {
        return &(mApp.lock()->mPackageName);
    }
    return nullptr;
}

const char* ServiceRecord::status2Str(const int status) {
    switch (status) {
        case ServiceRecord::CREATING:
            return "creating";
        case ServiceRecord::CREATED:
            return "created";
        case ServiceRecord::STARTING:
            return "starting";
        case ServiceRecord::STARTED:
            return "started";
        case ServiceRecord::DESTROYING:
            return "destroying";
        case ServiceRecord::DESTROYED:
            return "destroyed";
        default:
            return "undefined";
    }
}

ServiceHandler ServiceList::findService(const string& packageName, const string& serviceName) {
    for (auto it : mServiceList) {
        if (it->mServiceName == serviceName) {
            const string* package = it->getPackageName();
            if (package && *package == packageName) {
                return it;
            }
        }
    }
    return nullptr;
}

ServiceHandler ServiceList::getService(const sp<IBinder>& token) {
    for (auto it : mServiceList) {
        if (it->mToken == token) {
            return it;
        }
    }
    return nullptr;
}

void ServiceList::addService(const ServiceHandler& service) {
    mServiceList.emplace_back(service);
}

void ServiceList::deleteService(const sp<IBinder>& token) {
    for (auto it : mServiceList) {
        if (it->mToken == token) {
            it = mServiceList.back();
            mServiceList.pop_back();
        }
    }
}

void ServiceList::unbindConnection(const sp<IServiceConnection>& conn) {
    // It's inefficient, but affordable
    for (auto& iter : mServiceList) {
        iter->unbind(conn);
    }
}

} // namespace am
} // namespace os