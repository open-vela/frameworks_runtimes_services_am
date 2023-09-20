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

#include <binder/IBinder.h>

#include <iostream>
#include <memory>

#include "app/Intent.h"
#include "os/wm/BnWindowManager.h"

namespace os {
namespace am {

using android::IBinder;
using android::sp;
using os::app::Intent;

class AppRecord;
class ActivityStack;

using ActivityStackHandler = std::shared_ptr<ActivityStack>;

class ActivityRecord : public std::enable_shared_from_this<ActivityRecord> {
public:
    enum Status {
        ERROR = -1,
        CREATING = 0,
        CREATED,
        STARTING,
        STARTED,
        RESUMING,
        RESUMED,
        PAUSING,
        PAUSED,
        STOPPING,
        STOPPED,
        DESTROYING,
        DESTROYED,
    };

    enum LaunchMode { STANDARD, SINGLE_TOP, SINGLE_TASK, SINGLE_INSTANCE };

    ActivityRecord(const std::string& name, const sp<IBinder>& caller, const int32_t requestCode,
                   const LaunchMode launchMode, const ActivityStackHandler& task,
                   const Intent& intent, sp<::os::wm::IWindowManager> wm);

    void create();
    void start();
    void resume();
    void pause();
    void stop();
    void destroy();

    void abnormalExit();
    void onResult(int32_t requestCode, int32_t resultCode, const Intent& resultData);

    const sp<IBinder>& getToken() const;
    const std::string& getName() const;
    LaunchMode getLaunchMode() const;

    const sp<IBinder>& getCaller() const;
    int32_t getRequestCode() const;
    ActivityStackHandler getTask() const;

    void setAppThread(const std::shared_ptr<AppRecord>& app);
    std::shared_ptr<AppRecord> getAppRecord() const;

    void setIntent(const Intent& intent);
    const Intent& getIntent() const;

    void setStatus(Status status);
    Status getStatus() const;

    const std::string* getPackageName() const;

    const char* getStatusStr() const;
    static const char* statusToStr(const int status);
    static LaunchMode launchModeToInt(const std::string& launchModeStr);

    friend std::ostream& operator<<(std::ostream& os, const ActivityRecord& record);

private:
    std::string mName;
    sp<IBinder> mToken;
    sp<IBinder> mCaller;
    int32_t mRequestCode;
    Status mStatus;
    LaunchMode mLaunchMode;
    std::weak_ptr<AppRecord> mApp;
    std::weak_ptr<ActivityStack> mInTask;
    Intent mIntent;
    sp<::os::wm::IWindowManager> mWindowService;
};

using ActivityHandler = std::shared_ptr<ActivityRecord>;

} // namespace am
} // namespace os