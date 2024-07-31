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

#include <pm/PackageInfo.h>

#include <string>
#include <vector>

#include "TaskBoard.h"
#include "app/Intent.h"
#include "os/app/IServiceConnection.h"

namespace os {
namespace am {

using android::IBinder;
using android::sp;
using os::app::Intent;
using os::app::IServiceConnection;
using os::pm::ProcessPriority;

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

    ServiceRecord(const std::string& name, const sp<IBinder>& token, const ProcessPriority priority,
                  const std::shared_ptr<AppRecord>& appRecord)
          : mServiceName(name),
            mToken(token),
            mServiceBinder(nullptr),
            mStatus(CREATING),
            mStartFlag(F_UNKNOW),
            mPriority(priority),
            mApp(appRecord) {}

    void start(const Intent& intent);
    void stop();
    void bind(const sp<IBinder>& caller, const sp<IServiceConnection>& conn, const Intent& intent);
    void unbind(const sp<IServiceConnection>& conn);

    void abnormalExit();
    const std::string* getPackageName() const;
    int getPid() const; 
    bool isAlive();
    static const char* statusToStr(int status);

public:
    const std::string mServiceName;
    sp<IBinder> mToken;
    sp<IBinder> mServiceBinder;
    std::vector<sp<IServiceConnection>> mConnectRecord;
    int mStatus;
    int mStartFlag; // started or binded
    ProcessPriority mPriority;
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

    friend std::ostream& operator<<(std::ostream& os, const ServiceList& services);

private:
    std::vector<ServiceHandler> mServiceList;
};

class ServiceReportStatusTask : public Task {
public:
    struct Event : Label {
        sp<android::IBinder> mToken;
        Event(const int status, const sp<android::IBinder>& token)
              : Label(SERVICE_STATUS_BASE + status), mToken(token) {}
    };

    using TaskFunc = std::function<void()>;
    ServiceReportStatusTask(const int status, const sp<android::IBinder>& token, const TaskFunc& cb)
          : Task(SERVICE_STATUS_BASE + status), mToken(token), mCallback(cb) {}

    bool operator==(const Label& e) const {
        if (mId == e.mId) {
            return mToken == static_cast<const Event*>(&e)->mToken;
        }
        return false;
    }

    void execute(const Label& e) override {
        mCallback();
    }

private:
    sp<android::IBinder> mToken;
    TaskFunc mCallback;
};

} // namespace am
} // namespace os