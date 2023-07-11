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

namespace os {
namespace app {

void Service::setServiceName(const string& name) {
    mServiceName = name;
}

const std::string& Service::getServiceName() {
    return mServiceName;
}

void Service::reportServiceStatus(int status) {
    const std::string target = getPackageName().append("/").append(mServiceName);
    getActivityManager().reportServiceStatus(target, status);
}

} // namespace app
} // namespace os