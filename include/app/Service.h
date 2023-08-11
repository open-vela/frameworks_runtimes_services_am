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

#include <string>

#include "app/Context.h"
#include "os/app/IServiceConnection.h"

namespace os {
namespace app {

using os::app::IServiceConnection;

class Service : public ContextWrapper {
public:
    Service();
    virtual ~Service() = default;
    /** The status is part of the ServiceRecord inside */
    enum {
        CREATED = 1,
        STARTED = 3,
        BINDED = 5,
        UNBINDED = 7,
        DESTROYED = 9,
    };

    virtual void onCreate() = 0;
    virtual void onStartCommand(const Intent& intent) = 0;
    virtual void onDestory() = 0;
    virtual sp<IBinder> onBind(const Intent& intent);
    virtual bool onUnbind();

    // TODO set this function private.
    void setServiceName(const string& name);
    const std::string& getServiceName();
    int bindService(const Intent& intent, const sp<IServiceConnection>& conn);
    void unbindService();
    void reportServiceStatus(int status);

private:
    std::string mServiceName;
    sp<IBinder> mServiceBinder;
    bool mIsBinded;
};

} // namespace app
} // namespace os