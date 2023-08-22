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
#pragma once

#include <app/Service.h>

namespace os {
namespace app {

class ServiceClientRecord {
public:
    ServiceClientRecord(const string& name, const std::shared_ptr<Service>& service);
    ~ServiceClientRecord();

    /** The status is part of the ServiceRecord inside */
    enum {
        CREATING = 0,
        CREATED,
        STARTING,
        STARTED,
        BINDING,
        BINDED,
        UNBINDING,
        UNBINDED,
        DESTROYING,
        DESTROYED,
    };

    enum {
        F_UNKNOW = 0,
        F_STARTED = 0b1,
        F_BINDED = 0b10,
    };

    void onStart(const Intent& intent);
    void onBind(const Intent& intent, const sp<IServiceConnection>& conn);
    void onUnbind();
    void onDestroy();

    void reportServiceStatus(const int32_t status);
    const sp<IBinder>& getToken();

private:
    string mServiceName;
    std::shared_ptr<Service> mService;
    int32_t mStatus;
    int mStartFlag; // started | binded
};

} // namespace app
} // namespace os