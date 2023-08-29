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

class ActivityRecord : public std::enable_shared_from_this<ActivityRecord> {
public:
    ActivityRecord(const std::string& name, const sp<IBinder>& token, const sp<IBinder>& caller,
                   const int32_t requestCode, const std::string& launchMode,
                   const std::shared_ptr<AppRecord>& app,
                   const std::shared_ptr<ActivityStack>& task, sp<::os::wm::IWindowManager> wm)
          : mActivityName(name),
            mToken(token),
            mCaller(caller),
            mRequestCode(requestCode),
            mStatus(CREATING),
            mLaunchMode(launchMode),
            mApp(app),
            mInTask(task),
            mWindowService(wm) {}

    enum {
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

    void create();
    void start();
    void resume();
    void pause();
    void stop();
    void destroy();
    void onResult(int32_t requestCode, int32_t resultCode, const Intent& resultData);

    const std::string* getPackageName() const;
    static const char* status2Str(const int status);

    friend std::ostream& operator<<(std::ostream& os, const ActivityRecord& record);

public:
    std::string mActivityName;
    sp<IBinder> mToken;
    sp<IBinder> mCaller;
    int32_t mRequestCode;
    int mStatus;
    std::string mLaunchMode;
    std::weak_ptr<AppRecord> mApp;
    std::weak_ptr<ActivityStack> mInTask;
    Intent mIntent;
    sp<::os::wm::IWindowManager> mWindowService;
};

using ActivityHandler = std::shared_ptr<ActivityRecord>;

} // namespace am

} // namespace os
