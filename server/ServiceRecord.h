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

#include <string>
#include <vector>

#include "app/Intent.h"
#include "os/app/IServiceConnection.h"

namespace os {
namespace am {

using android::IBinder;
using android::sp;
using os::app::Intent;
using os::app::IServiceConnection;

class AppRecord;

class ServiceRecord : public std::enable_shared_from_this<ServiceRecord> {
public:
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

    ServiceRecord(const std::string& name, const sp<IBinder>& token,
                  const std::shared_ptr<AppRecord>& appRecord)
          : mServiceName(name),
            mToken(token),
            mServiceBinder(nullptr),
            mStatus(CREATING),
            mStartFlag(F_UNKNOW),
            mApp(appRecord) {}

    void start(const Intent& intent);
    void stop();
    void bind(const sp<IBinder>& caller, const sp<IServiceConnection>& conn, const Intent& intent);
    void unbind(const sp<IServiceConnection>& conn);

    void abnormalExit();
    const std::string* getPackageName();
    bool isAlive();
    static const char* status2Str(int status);

public:
    const std::string mServiceName;
    sp<IBinder> mToken;
    sp<IBinder> mServiceBinder;
    std::vector<sp<IServiceConnection>> mConnectRecord;
    int mStatus;
    int mStartFlag; // started or binded
    std::weak_ptr<AppRecord> mApp;
};

using ServiceHandler = std::shared_ptr<ServiceRecord>;

class ServiceList {
public:
    ServiceHandler findService(const std::string& packageName, const std::string& serviceName);
    ServiceHandler getService(const sp<IBinder>& token);
    void addService(const ServiceHandler& service);
    void deleteService(const sp<IBinder>& token);
    void unbindConnection(const sp<IServiceConnection>& conn);

private:
    std::vector<ServiceHandler> mServiceList;
};

} // namespace am
} // namespace os