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
#include "os/app/IServiceConnection.h"

namespace os {
namespace app {

using android::IBinder;
using android::sp;
using os::am::IActivityManager;
using os::app::Intent;
using os::app::IServiceConnection;
using std::string;

class ActivityManager {
public:
    ActivityManager() = default;
    ~ActivityManager() = default;

    static inline const char* name() {
        return "activity";
    }

    /** define constant macro */
    static const int NO_REQUEST = -1;
    static const int RESULT_OK = 0;
    static const int RESULT_CANCEL = -1;

    /** The status is part of AMS inside */
    enum {
        CREATED = 1,
        STARTED = 3,
        RESUMED = 5,
        PAUSED = 7,
        STOPPED = 9,
        DESTROYED = 11,
    };

    int32_t attachApplication(const sp<os::app::IApplicationThread>& app);
    int32_t startActivity(const sp<IBinder>& token, const Intent& intent, int32_t requestCode);
    int32_t stopActivity(const Intent& intent, int32_t resultCode);

    bool finishActivity(const sp<IBinder>& token, int32_t resultCode,
                        const std::shared_ptr<Intent>& resultData);
    void reportActivityStatus(const sp<IBinder>& token, int32_t status);

    int32_t startService(const Intent& intent);
    int32_t stopService(const Intent& intent);
    void reportServiceStatus(const sp<IBinder>& token, int32_t status);
    int32_t bindService(const sp<IBinder>& token, const Intent& intent,
                        const sp<IServiceConnection>& conn);
    void unbindService(const sp<IServiceConnection>& conn);
    void publishService(const sp<IBinder>& token, const sp<IBinder>& serviceHandler);
    int32_t stopServiceByToken(const sp<IBinder>& token);

    int32_t sendBroadcast(const Intent& intent);
    int32_t registerReceiver(const std::string& action, const sp<IBroadcastReceiver>& receiver);
    void unregisterReceiver(const sp<IBroadcastReceiver>& receiver);

    sp<IActivityManager> getService();

private:
    std::mutex mLock;
    sp<IActivityManager> mService;
};

} // namespace app
} // namespace os