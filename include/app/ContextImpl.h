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
    ContextImpl() = default;
    ~ContextImpl() = default;

    ContextImpl(Application* app, const sp<IBinder>& token);

    string getPackageName() override;
    static std::shared_ptr<Context> createActivityContext(Application* app,
                                                          const sp<IBinder>& token);

    void startActivity(const Intent& intent) override;
    void startActivityForResult(const Intent& intent, int32_t requestCode) override;
    void startService(const Intent& intent) override;

    void reportActivityStatus(const int status) override;

    void setIntent(const Intent& intent) override;
    const Intent& getIntent() override;

private:
    Application* mApp;
    ActivityManager mAm;

    sp<IBinder> mToken;
    Intent mIntent;
};

} // namespace app
} // namespace os