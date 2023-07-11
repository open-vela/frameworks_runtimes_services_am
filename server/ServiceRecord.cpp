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

#include <optional>

#include "AppRecord.h"

namespace os {
namespace am {

using std::string;

void ServiceRecord::start(const Intent& intent) {
    if (!mApp.expired()) {
        (mApp.lock()->mAppThread)->scheduleStartService(mServiceName, intent);
    }
}

void ServiceRecord::stop() {
    if (!mApp.expired()) {
        (mApp.lock()->mAppThread)->scheduleStopService(mServiceName);
    }
}

const string* ServiceRecord::getPackageName() {
    if (!mApp.expired()) {
        return &(mApp.lock()->mPackageName);
    }
    return nullptr;
}

ServiceHandler ServiceList::getService(const std::string& packageName,
                                       const std::string& serviceName) {
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

void ServiceList::addService(const ServiceHandler& service) {
    mServiceList.emplace_back(service);
}

void ServiceList::deleteService(const std::string& packageName, const std::string& serviceName) {
    for (auto it : mServiceList) {
        if (it->mServiceName == serviceName) {
            const string* package = it->getPackageName();
            if (package && *package == packageName) {
                it = mServiceList.back();
                mServiceList.pop_back();
            }
        }
    }
}

} // namespace am
} // namespace os