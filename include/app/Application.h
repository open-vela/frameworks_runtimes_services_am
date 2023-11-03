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

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "app/Activity.h"
#include "app/Service.h"
#include "app/UvLoop.h"
#include "os/app/IApplicationThread.h"

namespace os {

namespace wm {
class WindowManager;
}

namespace app {

using android::IBinder;
using android::sp;
using std::map;
using std::string;
using std::vector;

using CreateActivityFunc = std::function<Activity*(void)>;
using CreateServiceFunc = std::function<Service*(void)>;

class ActivityClientRecord;
class ServiceClientRecord;
class ApplicationThreadStub;

#define REGISTER_ACTIVITY(classname) \
    registerActivity(#classname, []() -> os::app::Activity* { return new classname; });

#define REGISTER_SERVICE(classname) \
    registerService(#classname, []() -> os::app::Service* { return new classname; });

class Application {
public:
    Application();
    virtual ~Application();

    virtual void onCreate() = 0;
    virtual void onForeground() = 0;
    virtual void onBackground() = 0;
    virtual void onDestroy() = 0;
    virtual void onReceiveIntent(const Intent& intent){};

    const string& getPackageName() const;
    void setPackageName(const string& name);
    int getUid() {
        return mUid;
    }
    void setMainLoop(UvLoop* loop) {
        mMainLoop = loop;
    }
    UvLoop* getMainLoop() const {
        return mMainLoop;
    }
    bool isSystemUI() const;

    void registerActivity(const string& name, const CreateActivityFunc& createFunc);
    void registerService(const string& name, const CreateServiceFunc& createFunc);

    ::os::wm::WindowManager* getWindowManager();

private:
    friend class ApplicationThread;
    friend class ApplicationThreadStub;
    std::shared_ptr<Activity> createActivity(const string& name);
    std::shared_ptr<Service> createService(const string& name);

    void addActivity(const sp<IBinder>& token,
                     const std::shared_ptr<ActivityClientRecord>& activity);
    std::shared_ptr<ActivityClientRecord> findActivity(const sp<IBinder>& token);
    void deleteActivity(const sp<IBinder>& token);

    void addService(const std::shared_ptr<ServiceClientRecord>& service);
    std::shared_ptr<ServiceClientRecord> findService(const sp<IBinder>& token);
    void deleteService(const sp<IBinder>& token);

    void clearActivityAndService();

private:
    map<sp<IBinder>, std::shared_ptr<ActivityClientRecord>> mExistActivities;
    vector<std::shared_ptr<ServiceClientRecord>> mExistServices;
    string mPackageName;
    map<string, CreateActivityFunc> mActivityMap;
    map<string, CreateServiceFunc> mServiceMap;
    int mUid;
    int mPid;
    UvLoop* mMainLoop;
    ::os::wm::WindowManager* mWindowManager;
};

} // namespace app
} // namespace os
