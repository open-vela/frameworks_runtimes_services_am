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

#include "app/Application.h"

#include <sys/types.h>
#include <unistd.h>
#include <utils/Log.h>

#include "ActivityClientRecord.h"
#include "ServiceClientRecord.h"
#include "WindowManager.h"
#include "app/ActivityManager.h"

namespace os {
namespace app {

using ::os::wm::WindowManager;

Application::Application() {
    mUid = getuid();
    mPid = getpid();
    mWindowManager = NULL;
}

Application::~Application() {
    if (mWindowManager) {
        delete mWindowManager;
        mWindowManager = NULL;
    }
}

const string& Application::getPackageName() const {
    return mPackageName;
}

void Application::setPackageName(const string& name) {
    mPackageName = name;
}

void Application::registerActivity(const string& name, const CreateActivityFunc& createFunc) {
    mActivityMap.emplace(name, createFunc);
    ALOGD("Application registerActivity:%s", name.c_str());
}

std::shared_ptr<Activity> Application::createActivity(const string& name) {
    auto it = mActivityMap.find(name);
    if (it != mActivityMap.end()) {
        return std::shared_ptr<Activity>(it->second());
    }
    ALOGE("Application createActivity failed:%s", name.c_str());
    return nullptr;
}

void Application::registerService(const string& name, const CreateServiceFunc& createFunc) {
    mServiceMap.emplace(name, createFunc);
    ALOGD("Application registerService:%s", name.c_str());
}

std::shared_ptr<Service> Application::createService(const string& name) {
    auto it = mServiceMap.find(name);
    if (it != mServiceMap.end()) {
        const auto service = std::shared_ptr<Service>(it->second());
        return service;
    }
    ALOGE("Application createService failed:%s", name.c_str());
    return nullptr;
}

void Application::addActivity(const sp<IBinder>& token,
                              const std::shared_ptr<ActivityClientRecord>& activity) {
    mExistActivities.insert({token, activity});
}

std::shared_ptr<ActivityClientRecord> Application::findActivity(const sp<IBinder>& token) {
    auto it = mExistActivities.find(token);
    if (it != mExistActivities.end()) {
        return it->second;
    } else {
        return nullptr;
    }
}

void Application::deleteActivity(const sp<IBinder>& token) {
    auto it = mExistActivities.find(token);
    if (it != mExistActivities.end()) {
        mExistActivities.erase(it);
    }
}

void Application::addService(const std::shared_ptr<ServiceClientRecord>& service) {
    mExistServices.push_back(service);
}

std::shared_ptr<ServiceClientRecord> Application::findService(const sp<IBinder>& token) {
    for (auto it : mExistServices) {
        if (it->getToken() == token) {
            return it;
        }
    }
    return nullptr;
}

void Application::deleteService(const sp<IBinder>& token) {
    for (auto& it : mExistServices) {
        if (it->getToken() == token) {
            it = mExistServices.back();
            mExistServices.pop_back();
            break;
        }
    }
}

void Application::clearActivityAndService() {
    for (auto it : mExistActivities) {
        const auto status = (it.second)->getStatus();
        if (status >= ActivityClientRecord::STARTED && status <= ActivityClientRecord::PAUSED) {
            it.second->onStop();
        }
        if (status < ActivityClientRecord::DESTROYING) {
            it.second->onDestroy();
        }
    }
    mExistServices.clear();

    for (auto it : mExistServices) {
        if (it->getStatus() < ServiceClientRecord::DESTROYING) {
            it->onDestroy();
        }
    }
    mExistServices.clear();
}

WindowManager* Application::getWindowManager() {
    if (mWindowManager == NULL) {
        mWindowManager = new WindowManager();
    }
    return mWindowManager;
}

} // namespace app
} // namespace os
