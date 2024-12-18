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

#include "app/ActivityManager.h"
#include "app/Intent.h"
#include "app/ServiceConnection.h"
#include "app/UvLoop.h"

namespace os {

namespace wm {
class WindowManager;
}

namespace app {

class Application;

class Context {
public:
    Context() = default;
    virtual ~Context() = default;

    virtual const Application* getApplication() const = 0;
    virtual const string& getPackageName() const = 0;
    virtual const string& getComponentName() const = 0;
    virtual UvLoop* getMainLoop() const = 0;
    virtual UvLoop* getCurrentLoop() const = 0;
    virtual const sp<IBinder>& getToken() const = 0;
    virtual ActivityManager& getActivityManager() = 0;
    virtual ::os::wm::WindowManager* getWindowManager() = 0;

    virtual int32_t stopApplication() = 0;
    virtual int32_t startActivity(const Intent& intent) = 0;
    virtual int32_t startActivityForResult(const Intent& intent, int32_t requestCode) = 0;
    virtual int32_t stopActivity(const Intent& intent) = 0;

    virtual int32_t startService(const Intent& intent) = 0;
    virtual int32_t stopService(const Intent& intent) = 0;
    virtual int32_t stopService() = 0;
    virtual int bindService(const Intent& intent, const sp<IServiceConnection>& conn) = 0;
    virtual void unbindService(const sp<IServiceConnection>& conn) = 0;

    virtual int32_t postIntent(const Intent& intent) = 0;
    virtual int32_t sendBroadcast(const Intent& intent) = 0;
    virtual int32_t registerReceiver(const std::string& action,
                                     const sp<IBroadcastReceiver>& receiver) = 0;
    virtual void unregisterReceiver(const sp<IBroadcastReceiver>& receiver) = 0;

    virtual void setIntent(const Intent& intent) = 0;
    virtual const Intent& getIntent() = 0;
};

class ContextWrapper : public Context {
public:
    std::shared_ptr<Context> getContext();
    void attachBaseContext(const std::shared_ptr<Context>& base);

    const Application* getApplication() const;
    const string& getPackageName() const override;
    const string& getComponentName() const override;

    UvLoop* getMainLoop() const;
    UvLoop* getCurrentLoop() const;
    const sp<IBinder>& getToken() const;
    ActivityManager& getActivityManager();
    ::os::wm::WindowManager* getWindowManager();

    int32_t stopApplication() override;
    int32_t startActivity(const Intent& intent) override;
    int32_t startActivityForResult(const Intent& intent, int32_t requestCode) override;
    int32_t stopActivity(const Intent& intent) override;

    int32_t startService(const Intent& intent) override;
    int32_t stopService(const Intent& intent) override;
    int32_t stopService() override;
    int bindService(const Intent& intent, const sp<IServiceConnection>& conn) override;
    void unbindService(const sp<IServiceConnection>& conn) override;

    int32_t postIntent(const Intent& intent) override;
    int32_t sendBroadcast(const Intent& intent) override;
    int32_t registerReceiver(const std::string& action,
                             const sp<IBroadcastReceiver>& receiver) override;
    void unregisterReceiver(const sp<IBroadcastReceiver>& receiver) override;

    void setIntent(const Intent& intent);
    const Intent& getIntent();

protected:
    std::shared_ptr<Context> mBase;
};

} // namespace app
} // namespace os
