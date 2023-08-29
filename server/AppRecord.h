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

#include <memory>
#include <string>
#include <vector>

#include "ActivityRecord.h"
#include "ServiceRecord.h"
#include "os/app/IApplicationThread.h"

namespace os {
namespace am {

using android::sp;
using os::app::IApplicationThread;

struct AppRecord {
    sp<IApplicationThread> mAppThread;
    std::string mPackageName;
    int mPid;
    int mUid;
    std::vector<std::weak_ptr<ActivityRecord>> mExistActivity;
    std::vector<std::weak_ptr<ServiceRecord>> mExistService;

    AppRecord(sp<IApplicationThread> app, std::string packageName, int pid, int uid)
          : mAppThread(app), mPackageName(packageName), mPid(pid), mUid(uid) {}

    ActivityHandler checkActivity(const std::string& activityName);
    ServiceHandler checkService(const std::string& serviceName);

    int checkActiveStatus();

    void addActivity(const std::shared_ptr<ActivityRecord>& activity);
    int deleteActivity(const std::shared_ptr<ActivityRecord>& activity);
    void addService(const std::shared_ptr<ServiceRecord>& service);
    int deleteService(const std::shared_ptr<ServiceRecord>& service);
};

class AppInfoList {
public:
    const std::shared_ptr<AppRecord> findAppInfo(const int pid);
    const std::shared_ptr<AppRecord> findAppInfo(const std::string& packageName);
    bool addAppInfo(const std::shared_ptr<AppRecord>& appInfo);
    void deleteAppInfo(const int pid);
    void deleteAppInfo(const std::string& packageName);

private:
    std::vector<std::shared_ptr<AppRecord>> mAppList;
};

} // namespace am
} // namespace os
