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
#include <mutex>
#include <string>

#include "app/Intent.h"
#include "os/am/IActivityManager.h"
#include "os/app/IApplicationThread.h"

namespace os {
namespace app {

using android::IBinder;
using android::sp;
using os::am::IActivityManager;
using os::app::Intent;

class ActivityManager {
public:
    ActivityManager() = default;
    ~ActivityManager() = default;

    static inline const char* name() {
        return "activity";
    }
    static const int NO_REQUEST = -1;
    /** The status is part of AMS inside */
    enum {
        CREATED = 1,
        STARTED = 3,
        RESUMED = 5,
        PAUSED = 7,
        STOPPED = 9,
        DESTORYED = 11,
    };

    int attachApplication(const sp<os::app::IApplicationThread>& app);
    int startActivity(const sp<IBinder>& token, const Intent& intent, int32_t requestCode);
    bool finishActivity(const sp<IBinder>& token, int32_t resultCode,
                        const std::shared_ptr<Intent>& resultData);
    void reportActivityStatus(const sp<IBinder>& token, int32_t status);
    int startService(const sp<IBinder>& token, const Intent& intent);

private:
    std::mutex mLock;
    sp<IActivityManager> mService;
    sp<IActivityManager> getService();
};

} // namespace app
} // namespace os