
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

/**
 * @class ServiceConnection
 * @brief A class that handles the connection and disconnection events of a service.
 */
class ServiceConnection : public BnServiceConnection {
public:
    /**
     * @brief Called when the service is successfully connected.
     *
     * @param[in] server The IBinder object representing the service that has been connected.
     */
    virtual void onConnected(const sp<IBinder>& server) = 0;
    /**
     * @brief Called when the service is disconnected.
     *
     * @param[in] server The IBinder object representing the service that has been disconnected.
     */
    virtual void onDisconnected(const sp<IBinder>& server) = 0;

private:
    /**
     * @brief Internal method that is invoked when the service is connected.
     *
     * @param[in] server The IBinder object representing the connected service.
     * @return The status of the connection (always returns Status::ok()).
     */
    Status onServiceConnected(const sp<IBinder>& server) override {
        onConnected(server);
        return Status::ok();
    }
    /**
     * @brief Internal method that is invoked when the service is disconnected.
     *
     * @param[in] server The IBinder object representing the disconnected service.
     * @return The status of the disconnection (always returns Status::ok()).
     */
    Status onServiceDisconnected(const sp<IBinder>& server) override {
        onDisconnected(server);
        return Status::ok();
    };
};

} // namespace app
} // namespace os
