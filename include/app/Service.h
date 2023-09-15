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

class ServiceClientRecord;

class Service : public ContextWrapper {
public:
    Service();
    virtual ~Service() = default;

    virtual void onCreate() = 0;
    virtual void onStartCommand(const Intent& intent) = 0;
    virtual void onDestroy() = 0;
    virtual sp<IBinder> onBind(const Intent& intent);
    virtual bool onUnbind();

private:
    friend class ServiceClientRecord;
    int bind(const Intent& intent, const sp<IServiceConnection>& conn);
    void unbind();

    sp<IBinder> mServiceBinder;
    bool mIsBinded;
};

} // namespace app
} // namespace os