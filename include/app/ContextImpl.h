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
#include "app/Application.h"
#include "app/Context.h"

namespace os {
namespace app {

class ContextImpl : public Context {
public:
    ContextImpl(const Application* app, const sp<IBinder>& token, UvLoop* loop);

    const Application* getApplication() const override;
    string getPackageName() override;
    UvLoop* getMainLoop() const override;
    UvLoop* getCurrentLoop() const override;
    const sp<IBinder>& getToken() const override;
    ActivityManager& getActivityManager() override;

    static std::shared_ptr<Context> createActivityContext(const Application* app,
                                                          const sp<IBinder>& token, UvLoop* loop);
    static std::shared_ptr<Context> createServiceContext(const Application* app,
                                                         const sp<IBinder>& token, UvLoop* loop);

    void startActivity(const Intent& intent) override;
    void startActivityForResult(const Intent& intent, int32_t requestCode) override;

    int32_t startService(const Intent& intent) override;
    int32_t stopService(const Intent& intent) override;
    int32_t stopService() override;
    int bindService(const Intent& intent, const sp<IServiceConnection>& conn) override;
    void unbindService(const sp<IServiceConnection>& conn) override;

    int32_t sendBroadcast(const Intent& intent) override;
    int32_t registerReceiver(const std::string& action,
                             const sp<IBroadcastReceiver>& receiver) override;
    void unregisterReceiver(const sp<IBroadcastReceiver>& receiver) override;

    void setIntent(const Intent& intent) override;
    const Intent& getIntent() override;

public:
    const Application* mApp;
    const sp<IBinder> mToken;
    UvLoop* mLoop;

    ActivityManager mAm;
    Intent mIntent;
};

} // namespace app
} // namespace os