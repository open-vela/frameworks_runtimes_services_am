
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

#include "os/app/BnServiceConnection.h"
#include "os/app/IServiceConnection.h"

namespace os {
namespace app {

using android::IBinder;
using android::sp;
using android::binder::Status;

class ServiceConnection : public BnServiceConnection {
public:
    virtual void onConnected(const sp<IBinder>& server) = 0;
    virtual void onDisconnected(const sp<IBinder>& server) = 0;

private:
    Status onServiceConnected(const sp<IBinder>& server) override {
        onConnected(server);
        return Status::ok();
    }
    Status onServiceDisconnected(const sp<IBinder>& server) override {
        onDisconnected(server);
        return Status::ok();
    };
};

} // namespace app
} // namespace os
