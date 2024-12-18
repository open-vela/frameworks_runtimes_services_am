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
    if (auto appRecord = mApp.lock()) {
        mStartFlag |= F_STARTED;
        appRecord->addService(shared_from_this());
        appRecord->mAppThread->scheduleStartService(mServiceName, mToken, intent);
    }
}

void ServiceRecord::stop() {
    for (auto iter : mConnectRecord) {
        iter->onServiceDisconnected(mServiceBinder);
    }
    if (auto appRecord = mApp.lock()) {
        appRecord->deleteService(shared_from_this());
        appRecord->mAppThread->scheduleStopService(mToken);
    }
}

void ServiceRecord::bind(const sp<IBinder>& caller, const sp<IServiceConnection>& conn,
                         const Intent& intent) {
    if (auto appRecord = mApp.lock()) {
        if (mStartFlag == F_UNKNOW) {
            appRecord->addService(shared_from_this());
        }
        mStartFlag |= F_BINDED;
        if (mServiceBinder) {
            conn->onServiceConnected(mServiceBinder);
        } else {
            appRecord->mAppThread->scheduleBindService(mServiceName, mToken, intent, conn);
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
        conn->onServiceDisconnected(mServiceBinder);
        const int size = mConnectRecord.size();
        for (int i = 0; i < size; ++i) {
            if (android::IInterface::asBinder(mConnectRecord[i]) ==
                android::IInterface::asBinder(conn)) {
                mConnectRecord[i] = mConnectRecord[size - 1];
                mConnectRecord.pop_back();
                break;
            }
        }
        if (auto appRecord = mApp.lock()) {
            if (mConnectRecord.empty()) {
                mStartFlag &= ~F_BINDED;
                appRecord->mAppThread->scheduleUnbindService(mToken);
            }
            if (mStartFlag == F_UNKNOW) {
                appRecord->deleteService(shared_from_this());
            }
        }
    }
}

void ServiceRecord::abnormalExit() {
    for (auto iter : mConnectRecord) {
        iter->onServiceDisconnected(mServiceBinder);
    }
    if (auto appRecord = mApp.lock()) {
        ALOGW("Service:%s/%s abnormal exit!", mApp.lock()->mPackageName.c_str(),
              mServiceName.c_str());
        appRecord->deleteService(shared_from_this());
    }
}

bool ServiceRecord::isAlive() {
    return mStartFlag != F_UNKNOW;
}

const string* ServiceRecord::getPackageName() const {
    if (auto appRecord = mApp.lock()) {
        return &appRecord->mPackageName;
    }
    return nullptr;
}

int ServiceRecord::getPid() const {
    if (auto appRecord = mApp.lock()) {
        return appRecord->mPid;
    }
    return -1;
}

const char* ServiceRecord::statusToStr(const int status) {
    switch (status) {
        case ServiceRecord::CREATING:
            return "creating";
        case ServiceRecord::CREATED:
            return "created";
        case ServiceRecord::STARTING:
            return "starting";
        case ServiceRecord::STARTED:
            return "started";
        case ServiceRecord::BINDED:
            return "binded";
        case ServiceRecord::UNBINDED:
            return "unbinded";
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
    for (auto& it : mServiceList) {
        if (it->mToken == token) {
            it = mServiceList.back();
            mServiceList.pop_back();
            break;
        }
    }
}

void ServiceList::unbindConnection(const sp<IServiceConnection>& conn) {
    // It's inefficient, but affordable
    for (auto& iter : mServiceList) {
        iter->unbind(conn);
    }
}

std::ostream& operator<<(std::ostream& os, const ServiceList& services) {
    os << "\n\nServices Information:" << std::endl;
    for (const auto& serviceRecord : services.mServiceList) {
        if (serviceRecord->getPackageName() == nullptr) continue;
        os << "\t" << *serviceRecord->getPackageName() << "/" << serviceRecord->mServiceName << " [ "
           << serviceRecord->getPid() << " ]" << " |"
           << ((serviceRecord->mStartFlag & ServiceRecord::F_STARTED) ? "start|" : "")
           << ((serviceRecord->mStartFlag & ServiceRecord::F_BINDED) ? "binded|" : "") << " ["
           << ServiceRecord::statusToStr(serviceRecord->mStatus) << "]" << std::endl;
    }
    return os;
}

} // namespace am
} // namespace os