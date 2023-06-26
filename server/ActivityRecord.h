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

#include <memory>

#include "app/Intent.h"

namespace os {
namespace am {

using android::IBinder;
using android::sp;
using os::app::Intent;

class AppRecord;
class ActivityStack;

class ActivityRecord {
public:
    ActivityRecord(const std::string& name, const sp<IBinder>& token, const sp<IBinder>& caller,
                   const std::string& launchMode, const std::shared_ptr<AppRecord>& app,
                   const std::shared_ptr<ActivityStack>& task)
          : mActivityName(name),
            mToken(token),
            mCaller(caller),
            mStatus(CREATING),
            mLaunchMode(launchMode),
            mApp(app),
            mInTask(task) {}

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
        STOPED,
        DESTROYING,
        DESTROYED,
    };

    void resume();
    void pause();
    void stop();
    void destroy();

public:
    std::string mActivityName;
    sp<IBinder> mToken;
    sp<IBinder> mCaller;
    int mStatus;
    std::string mLaunchMode;
    std::weak_ptr<AppRecord> mApp;
    std::weak_ptr<ActivityStack> mInTask;
    Intent mIntent;
};

using ActivityHandler = std::shared_ptr<ActivityRecord>;

} // namespace am

} // namespace os
