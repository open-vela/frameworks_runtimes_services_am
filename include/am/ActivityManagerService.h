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

#include <optional>
#include <string>

#include "os/am/BnActivityManager.h"
#include "os/am/IActivityManager.h"

namespace os {
namespace am {

using android::IBinder;
using android::sp;
using android::binder::Status;
using os::app::Intent;

class ActivityManagerInner;

class ActivityManagerService : public os::am::BnActivityManager {
public:
    ActivityManagerService();
    ~ActivityManagerService();
    /***binder api***/
    Status attachApplication(const sp<os::app::IApplicationThread>& app, int32_t* ret) override;

    Status startActivity(const sp<IBinder>& token, const Intent& intent, int32_t code,
                         int32_t* ret) override;
    Status finishActivity(const sp<IBinder>& token, int32_t resultCode,
                          const std::optional<Intent>& resultData, bool* ret) override;
    Status reportActivityStatus(const sp<IBinder>& token, int32_t status) override;

    Status startService(const Intent& intent, int32_t* ret) override;
    Status stopService(const Intent& intent, int32_t* ret) override;
    Status reportServiceStatus(const std::string& target, int32_t status) override;

    // The service is ready to start and the application can be launched
    void systemReady();

private:
    ActivityManagerInner* mInner;
};

} // namespace am
} // namespace os
