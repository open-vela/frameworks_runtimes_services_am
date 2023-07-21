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

#define LOG_TAG "App"

#include "app/ApplicationThread.h"

#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <fcntl.h>
#include <unistd.h>
#include <uv.h>

#include <mutex>

#include "app/Application.h"
#include "app/ContextImpl.h"
#include "os/app/BnApplicationThread.h"
#include "os/app/IApplicationThread.h"

namespace os {
namespace app {

using android::binder::Status;

/**
 * IApplicationThread.aidl interface service
 */
class ApplicationThreadStub : public os::app::BnApplicationThread {
public:
    ApplicationThreadStub() = default;
    ~ApplicationThreadStub() = default;

    void bind(Application* app) {
        mApp = app;
    }

    Status scheduleLaunchActivity(const string& activityName, const sp<IBinder>& token,
                                  const Intent& intent);
    Status scheduleStartActivity(const sp<IBinder>& token, const Intent& intent);
    Status scheduleResumeActivity(const sp<IBinder>& token, const Intent& intent);
    Status schedulePauseActivity(const sp<IBinder>& token);
    Status scheduleStopActivity(const sp<IBinder>& token);
    Status scheduleDestoryActivity(const sp<IBinder>& token);
    Status onActivityResult(const sp<IBinder>& token, int32_t requestCode, int32_t resultCode,
                            const Intent& data);

    Status scheduleStartService(const string& serviceName, const sp<IBinder>& token,
                                const Intent& intent);
    Status scheduleStopService(const sp<IBinder>& token);

private:
    int onLaunchActivity(const string& activityName, const sp<IBinder>& token,
                         const Intent& intent);
    int onStartActivity(const sp<IBinder>& token, const Intent& intent);
    int onResumeActivity(const sp<IBinder>& token, const Intent& intent);
    int onPauseActivity(const sp<IBinder>& token);
    int onStopActivity(const sp<IBinder>& token);
    int onDestoryActivity(const sp<IBinder>& token);
    int onStartService(const string& serviceName, const sp<IBinder>& token, const Intent& intent);
    int onStopService(const sp<IBinder>& token);

private:
    Application* mApp;
};

/**
 * ApplicationThread: Application's main thread
 */
ApplicationThread::ApplicationThread(Application* app) : mApp(app) {
    mApp->setMainLoop(this);
}

ApplicationThread::~ApplicationThread() {
    stop();
}

int ApplicationThread::mainRun(int argc, char** argv) {
    if (argc < 2) {
        ALOGE("illegally launch Application!!!");
        return -1;
    }
    ALOGI("start Application:%s execfile:%s", argv[1], argv[0]);
    android::ProcessState::self()->setThreadPoolMaxThreadCount(1);
    android::ProcessState::self()->startThreadPool();

    android::sp<ApplicationThreadStub> appThread(new ApplicationThreadStub);
    mApp->setPackageName(argv[1]);
    mApp->onCreate();
    appThread->bind(mApp);

    ActivityManager am;
    if (0 != am.attachApplication(appThread)) {
        ALOGE("ApplicationThread attach failure");
    }

    UvTimer tidle(*this, [](void*) { /*idle*/ });
    tidle.start(16, 30); /** cycle run, only for Prototype development */
    run();
    return 0;
}

Status ApplicationThreadStub::scheduleLaunchActivity(const std::string& activityName,
                                                     const sp<IBinder>& token,
                                                     const Intent& intent) {
    mApp->getMainLoop()->postTask([this, activityName, token, intent](void*) {
        this->onLaunchActivity(activityName, token, intent);
    });
    return Status::ok();
}

Status ApplicationThreadStub::scheduleStartActivity(const sp<IBinder>& token,
                                                    const Intent& intent) {
    mApp->getMainLoop()->postTask(
            [this, token, intent](void*) { this->onStartActivity(token, intent); });
    return Status::ok();
}

Status ApplicationThreadStub::scheduleResumeActivity(const sp<IBinder>& token,
                                                     const Intent& intent) {
    mApp->getMainLoop()->postTask(
            [this, token, intent](void*) { this->onResumeActivity(token, intent); });
    return Status::ok();
}

Status ApplicationThreadStub::schedulePauseActivity(const sp<IBinder>& token) {
    mApp->getMainLoop()->postTask([this, token](void*) { this->onPauseActivity(token); });
    return Status::ok();
}

Status ApplicationThreadStub::scheduleStopActivity(const sp<IBinder>& token) {
    mApp->getMainLoop()->postTask([this, token](void*) { this->onStopActivity(token); });
    return Status::ok();
}

Status ApplicationThreadStub::scheduleDestoryActivity(const sp<IBinder>& token) {
    mApp->getMainLoop()->postTask([this, token](void*) { this->onDestoryActivity(token); });
    return Status::ok();
}

Status ApplicationThreadStub::onActivityResult(const sp<IBinder>& token, int32_t requestCode,
                                               int32_t resultCode, const Intent& resultData) {
    /** Thinking: postTask causes the Data copy, it's necessary?  [oneway aidl interface]
     *  Do it immediately in here
     * */
    std::shared_ptr<Activity> activity = mApp->findActivity(token);
    if (activity) {
        activity->onActivityResult(requestCode, resultCode, resultData);
    }
    return Status::ok();
}

Status ApplicationThreadStub::scheduleStartService(const string& serviceName,
                                                   const sp<IBinder>& token, const Intent& intent) {
    mApp->getMainLoop()->postTask([this, serviceName, token, intent](void*) {
        this->onStartService(serviceName, token, intent);
    });
    return Status::ok();
}

Status ApplicationThreadStub::scheduleStopService(const sp<IBinder>& token) {
    mApp->getMainLoop()->postTask([this, token](void*) { this->onStopService(token); });
    return Status::ok();
}

int ApplicationThreadStub::onLaunchActivity(const std::string& activityName,
                                            const sp<IBinder>& token, const Intent& intent) {
    std::shared_ptr<Activity> activity = mApp->createActivity(activityName);
    if (activity != nullptr) {
        auto context = ContextImpl::createActivityContext(mApp, token);

        activity->attach(context, intent);

        mApp->addActivity(token, activity);

        activity->performCreate();
        activity->reportActivityStatus(ActivityManager::CREATED);

        return 0;
    }
    return -1;
}

int ApplicationThreadStub::onStartActivity(const sp<IBinder>& token, const Intent& intent) {
    std::shared_ptr<Activity> activity = mApp->findActivity(token);
    if (activity != nullptr) {
        if (activity->getStatus() == ActivityManager::STOPPED) {
            activity->onNewIntent(intent);
        }
        activity->performStart();
        activity->reportActivityStatus(ActivityManager::STARTED);
        return 0;
    }
    return -1;
}

int ApplicationThreadStub::onResumeActivity(const sp<IBinder>& token, const Intent& intent) {
    std::shared_ptr<Activity> activity = mApp->findActivity(token);
    if (activity != nullptr) {
        if (activity->getStatus() == ActivityManager::PAUSED) {
            activity->onNewIntent(intent);
        }
        activity->performResume();
        activity->reportActivityStatus(ActivityManager::RESUMED);
        return 0;
    }
    return -1;
}

int ApplicationThreadStub::onPauseActivity(const sp<IBinder>& token) {
    std::shared_ptr<Activity> activity = mApp->findActivity(token);
    if (activity != nullptr) {
        activity->performPause();
        activity->reportActivityStatus(ActivityManager::PAUSED);
        return 0;
    }
    return -1;
}

int ApplicationThreadStub::onStopActivity(const sp<IBinder>& token) {
    std::shared_ptr<Activity> activity = mApp->findActivity(token);
    if (activity != nullptr) {
        activity->performStop();
        activity->reportActivityStatus(ActivityManager::STOPPED);
        return 0;
    }
    return -1;
}

int ApplicationThreadStub::onDestoryActivity(const sp<IBinder>& token) {
    std::shared_ptr<Activity> activity = mApp->findActivity(token);
    if (activity != nullptr) {
        activity->performDestroy();
        activity->reportActivityStatus(ActivityManager::DESTORYED);
        mApp->deleteActivity(token);
        return 0;
    }
    return -1;
}

int ApplicationThreadStub::onStartService(const string& serviceName, const sp<IBinder>& token,
                                          const Intent& intent) {
    auto service = mApp->findService(token);
    if (service) {
        ALOGW("the %s had been started", serviceName.c_str());
        service->onStartCommand(intent);
        return 0;
    } else {
        service = mApp->createService(serviceName);
        if (!service) {
            ALOGW("the %s is non-existent", serviceName.c_str());
            return -1;
        }
        const auto context = ContextImpl::createServiceContext(mApp, token);
        service->attachBaseContext(context);
        mApp->addService(service);

        service->onCreate();
        service->reportServiceStatus(Service::CREATED);
        service->onStartCommand(intent);
        service->reportServiceStatus(Service::STARTED);
    }
    return 0;
}

int ApplicationThreadStub::onStopService(const sp<IBinder>& token) {
    auto service = mApp->findService(token);
    if (service) {
        service->onDestory();
        mApp->deleteService(token);
    }
    return 0;
}

} // namespace app
} // namespace os
