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
#include <utility>
#include <vector>

#include "ActivityRecord.h"
#include "ProcessPriorityPolicy.h"
#include "ServiceRecord.h"
#include "TaskBoard.h"
#include "os/app/IApplicationThread.h"

namespace os {
namespace am {

using android::sp;
using os::app::IApplicationThread;

class AppInfoList;

struct AppRecord {
    sp<IApplicationThread> mAppThread;
    std::string mPackageName;
    int mPid;
    int mUid;
    AppInfoList* mAppList;
    ProcessPriorityPolicy* mPriorityPolicy;
    int mForegroundActivityCnt;
    std::vector<std::weak_ptr<ActivityRecord>> mExistActivity;
    std::vector<std::weak_ptr<ServiceRecord>> mExistService;
    bool mIsAlive;

    AppRecord(sp<IApplicationThread> app, std::string packageName, int pid, int uid,
              AppInfoList* applist, ProcessPriorityPolicy* policy)
          : mAppThread(app),
            mPackageName(packageName),
            mPid(pid),
            mUid(uid),
            mAppList(applist),
            mPriorityPolicy(policy),
            mForegroundActivityCnt(0),
            mIsAlive(true) {}

    ActivityHandler checkActivity(const std::string& activityName);
    ServiceHandler checkService(const std::string& serviceName);

    bool checkActiveStatus() const;
    void stopApplication();
    void setForeground(const bool isForegroundActivity);
    void scheduleReceiveIntent(const sp<IBinder>& token, const Intent& intent);

    void addActivity(const std::shared_ptr<ActivityRecord>& activity);
    int deleteActivity(const std::shared_ptr<ActivityRecord>& activity);
    void addService(const std::shared_ptr<ServiceRecord>& service);
    int deleteService(const std::shared_ptr<ServiceRecord>& service);
};

class AppInfoList {
public:
    const std::shared_ptr<AppRecord> findAppInfo(const int pid);
    const std::shared_ptr<AppRecord> findAppInfoWithAlive(const int pid);
    const std::shared_ptr<AppRecord> findAppInfoWithAlive(const std::string& packageName);

    bool addAppInfo(const std::shared_ptr<AppRecord>& appInfo);
    void deleteAppInfo(const int pid);
    void deleteAppInfo(const std::string& packageName);

    void addAppWaitingAttach(const std::string& packageName, int pid);
    void deleteAppWaitingAttach(const int pid);
    int getAttachingAppPid(const std::string& packageName);
    bool getAttachingAppName(int pid, std::string& packageName);

private:
    std::vector<std::shared_ptr<AppRecord>> mAppList;
    // app had spawn but does't attach
    std::vector<std::pair<std::string, int>> mAppWaitingAttach;
};

class AppAttachTask : public Task {
public:
    struct Event : Label {
        const int mPid;
        const std::shared_ptr<AppRecord> mAppRecord;
        Event(int pid, const std::shared_ptr<AppRecord>& app)
              : Label(APP_ATTACH, LabelType::MULTI_TRIGGER), mPid(pid), mAppRecord(app) {}
    };

    using TaskFunc = std::function<void(const Event*)>;
    AppAttachTask(const int pid, const TaskFunc& cb) : Task(APP_ATTACH), mPid(pid), mCallback(cb) {}

    bool operator==(const Label& e) const {
        if (mId == e.mId) {
            return mPid == static_cast<const Event*>(&e)->mPid;
        }
        return false;
    }

    void execute(const Label& e) override {
        mCallback(static_cast<const Event*>(&e));
    }

private:
    int mPid;
    TaskFunc mCallback;
};

} // namespace am
} // namespace os
