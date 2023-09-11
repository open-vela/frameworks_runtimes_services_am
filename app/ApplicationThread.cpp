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

#include "ActivityClientRecord.h"
#include "Profiler.h"
#include "ServiceClientRecord.h"
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
    Status scheduleDestroyActivity(const sp<IBinder>& token);
    Status onActivityResult(const sp<IBinder>& token, int32_t requestCode, int32_t resultCode,
                            const Intent& data);

    Status scheduleStartService(const string& serviceName, const sp<IBinder>& token,
                                const Intent& intent);
    Status scheduleStopService(const sp<IBinder>& token);

    Status scheduleBindService(const string& serviceName, const sp<IBinder>& token,
                               const Intent& intent, const sp<IServiceConnection>& serviceBinder);
    Status scheduleUnbindService(const sp<IBinder>& token);

    Status terminateApplication();

private:
    int onLaunchActivity(const string& activityName, const sp<IBinder>& token,
                         const Intent& intent);
    int onStartActivity(const sp<IBinder>& token, const Intent& intent);
    int onResumeActivity(const sp<IBinder>& token, const Intent& intent);
    int onPauseActivity(const sp<IBinder>& token);
    int onStopActivity(const sp<IBinder>& token);
    int onDestroyActivity(const sp<IBinder>& token);
    int onStartService(const string& serviceName, const sp<IBinder>& token, const Intent& intent);
    int onStopService(const sp<IBinder>& token);

    void onBindService(const string& serviceName, const sp<IBinder>& token, const Intent& intent,
                       const sp<IServiceConnection>& serviceBinder);
    void onUnbindService(const sp<IBinder>& token);

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

    int binderFd;
    android::IPCThreadState::self()->setupPolling(&binderFd);
    if (binderFd < 0) {
        ALOGE("failed to open binder device:%d", errno);
        return -2;
    }
    UvPoll pollBinder(*this, binderFd);
    pollBinder.start(UV_READABLE, [](int fd, int status, int events, void* data) {
        android::IPCThreadState::self()->handlePolledCommands();
    });

    android::sp<ApplicationThreadStub> appThread(new ApplicationThreadStub);
    mApp->setPackageName(argv[1]);
    mApp->onCreate();
    appThread->bind(mApp);

    ActivityManager am;
    if (0 != am.attachApplication(appThread)) {
        ALOGE("ApplicationThread attach failure");
    }

    run();
    ALOGI("Application[%s]:%s has been stop!!!", argv[0], argv[1]);
    return 0;
}

Status ApplicationThreadStub::scheduleLaunchActivity(const std::string& activityName,
                                                     const sp<IBinder>& token,
                                                     const Intent& intent) {
    ALOGD("scheduleLaunchActivity package:%s activity:%s token[%p]", mApp->getPackageName().c_str(),
          activityName.c_str(), token.get());
    mApp->getMainLoop()->postTask([this, activityName, token, intent](void*) {
        this->onLaunchActivity(activityName, token, intent);
    });
    return Status::ok();
}

Status ApplicationThreadStub::scheduleStartActivity(const sp<IBinder>& token,
                                                    const Intent& intent) {
    ALOGD("scheduleStartActivity package:%s token[%p]", mApp->getPackageName().c_str(),
          token.get());
    mApp->getMainLoop()->postTask(
            [this, token, intent](void*) { this->onStartActivity(token, intent); });
    return Status::ok();
}

Status ApplicationThreadStub::scheduleResumeActivity(const sp<IBinder>& token,
                                                     const Intent& intent) {
    ALOGD("scheduleResumeActivity package:%s token[%p]", mApp->getPackageName().c_str(),
          token.get());
    mApp->getMainLoop()->postTask(
            [this, token, intent](void*) { this->onResumeActivity(token, intent); });
    return Status::ok();
}

Status ApplicationThreadStub::schedulePauseActivity(const sp<IBinder>& token) {
    ALOGD("schedulePauseActivity package:%s token[%p]", mApp->getPackageName().c_str(),
          token.get());
    mApp->getMainLoop()->postTask([this, token](void*) { this->onPauseActivity(token); });
    return Status::ok();
}

Status ApplicationThreadStub::scheduleStopActivity(const sp<IBinder>& token) {
    ALOGD("scheduleStopActivity package:%s token[%p]", mApp->getPackageName().c_str(), token.get());
    mApp->getMainLoop()->postTask([this, token](void*) { this->onStopActivity(token); });
    return Status::ok();
}

Status ApplicationThreadStub::scheduleDestroyActivity(const sp<IBinder>& token) {
    ALOGD("scheduleDestroyActivity package:%s token[%p]", mApp->getPackageName().c_str(),
          token.get());
    mApp->getMainLoop()->postTask([this, token](void*) { this->onDestroyActivity(token); });
    return Status::ok();
}

Status ApplicationThreadStub::scheduleBindService(const string& serviceName,
                                                  const sp<IBinder>& token, const Intent& intent,
                                                  const sp<IServiceConnection>& conn) {
    ALOGD("scheduleBindService token[%p]", token.get());
    mApp->getMainLoop()->postTask([this, serviceName, token, intent, conn](void*) {
        this->onBindService(serviceName, token, intent, conn);
    });
    return Status::ok();
}

Status ApplicationThreadStub::scheduleUnbindService(const sp<IBinder>& token) {
    ALOGD("scheduleUnbindService token[%p]", token.get());
    mApp->getMainLoop()->postTask([this, token](void*) { this->onUnbindService(token); });
    return Status::ok();
}

Status ApplicationThreadStub::onActivityResult(const sp<IBinder>& token, int32_t requestCode,
                                               int32_t resultCode, const Intent& resultData) {
    /** Thinking: postTask causes the Data copy, it's necessary?  [oneway aidl interface]
     *  Do it immediately in here
     * */
    ALOGD("onActivityResult package:%s token[%p]", mApp->getPackageName().c_str(), token.get());
    std::shared_ptr<ActivityClientRecord> activityRecord = mApp->findActivity(token);
    if (activityRecord) {
        activityRecord->onActivityResult(requestCode, resultCode, resultData);
    }
    return Status::ok();
}

Status ApplicationThreadStub::scheduleStartService(const string& serviceName,
                                                   const sp<IBinder>& token, const Intent& intent) {
    ALOGD("scheduleStartService package:%s service:%s token[%p]", mApp->getPackageName().c_str(),
          serviceName.c_str(), token.get());
    mApp->getMainLoop()->postTask([this, serviceName, token, intent](void*) {
        this->onStartService(serviceName, token, intent);
    });
    return Status::ok();
}

Status ApplicationThreadStub::scheduleStopService(const sp<IBinder>& token) {
    ALOGD("scheduleStopService package:%s token[%p]", mApp->getPackageName().c_str(), token.get());
    mApp->getMainLoop()->postTask([this, token](void*) { this->onStopService(token); });
    return Status::ok();
}

Status ApplicationThreadStub::terminateApplication() {
    ALOGD("terminateApplication package:%s", mApp->getPackageName().c_str());
    mApp->getMainLoop()->postTask([this](void*) {
        mApp->onDestroy();
        mApp->getMainLoop()->stop();
    });
    return Status::ok();
}

int ApplicationThreadStub::onLaunchActivity(const std::string& activityName,
                                            const sp<IBinder>& token, const Intent& intent) {
    AM_PROFILER_BEGIN();
    std::shared_ptr<Activity> activity = mApp->createActivity(activityName);
    if (activity != nullptr) {
        auto context = ContextImpl::createActivityContext(mApp, token, mApp->getMainLoop());
        activity->attach(context);
        auto activityRecord = std::make_shared<ActivityClientRecord>(activityName, activity);
        activityRecord->onCreate(intent);
        mApp->addActivity(token, activityRecord);
        AM_PROFILER_END();
        return 0;
    } else {
        ALOGE("the %s/%s is not register", mApp->getPackageName().c_str(), activityName.c_str());
    }
    AM_PROFILER_END();
    return -1;
}

int ApplicationThreadStub::onStartActivity(const sp<IBinder>& token, const Intent& intent) {
    AM_PROFILER_BEGIN();
    std::shared_ptr<ActivityClientRecord> activityRecord = mApp->findActivity(token);
    if (activityRecord != nullptr) {
        activityRecord->onStart(intent);
        AM_PROFILER_END();
        return 0;
    }
    AM_PROFILER_END();
    return -1;
}

int ApplicationThreadStub::onResumeActivity(const sp<IBinder>& token, const Intent& intent) {
    AM_PROFILER_BEGIN();
    std::shared_ptr<ActivityClientRecord> activityRecord = mApp->findActivity(token);
    if (activityRecord != nullptr) {
        activityRecord->onResume(intent);
        AM_PROFILER_END();
        return 0;
    }
    AM_PROFILER_END();
    return -1;
}

int ApplicationThreadStub::onPauseActivity(const sp<IBinder>& token) {
    AM_PROFILER_BEGIN();
    std::shared_ptr<ActivityClientRecord> activityRecord = mApp->findActivity(token);
    if (activityRecord != nullptr) {
        activityRecord->onPause();
        AM_PROFILER_END();
        return 0;
    }
    AM_PROFILER_END();
    return -1;
}

int ApplicationThreadStub::onStopActivity(const sp<IBinder>& token) {
    AM_PROFILER_BEGIN();
    std::shared_ptr<ActivityClientRecord> activityRecord = mApp->findActivity(token);
    if (activityRecord != nullptr) {
        activityRecord->onStop();
        AM_PROFILER_END();
        return 0;
    }
    AM_PROFILER_END();
    return -1;
}

int ApplicationThreadStub::onDestroyActivity(const sp<IBinder>& token) {
    AM_PROFILER_BEGIN();
    std::shared_ptr<ActivityClientRecord> activityRecord = mApp->findActivity(token);
    if (activityRecord != nullptr) {
        activityRecord->onDestroy();
        mApp->deleteActivity(token);
        AM_PROFILER_END();
        return 0;
    }
    AM_PROFILER_END();
    return -1;
}

int ApplicationThreadStub::onStartService(const string& serviceName, const sp<IBinder>& token,
                                          const Intent& intent) {
    AM_PROFILER_BEGIN();
    auto serviceRecord = mApp->findService(token);
    if (!serviceRecord) {
        auto service = mApp->createService(serviceName);
        if (!service) {
            ALOGW("the %s is non-existent", serviceName.c_str());
            AM_PROFILER_END();
            return -1;
        }
        const auto context = ContextImpl::createServiceContext(mApp, token, mApp->getMainLoop());
        service->attachBaseContext(context);
        serviceRecord = std::make_shared<ServiceClientRecord>(serviceName, service);
        mApp->addService(serviceRecord);
    }
    serviceRecord->onStart(intent);
    AM_PROFILER_END();
    return 0;
}

int ApplicationThreadStub::onStopService(const sp<IBinder>& token) {
    AM_PROFILER_BEGIN();
    auto serviceRecord = mApp->findService(token);
    if (serviceRecord) {
        serviceRecord->onDestroy();
    }
    AM_PROFILER_END();
    return 0;
}

void ApplicationThreadStub::onBindService(const string& serviceName, const sp<IBinder>& token,
                                          const Intent& intent,
                                          const sp<IServiceConnection>& conn) {
    AM_PROFILER_BEGIN();
    auto serviceRecord = mApp->findService(token);
    if (!serviceRecord) {
        auto service = mApp->createService(serviceName);
        if (!service) {
            ALOGE("the %s/%s is non-existent", mApp->getPackageName().c_str(), serviceName.c_str());
            AM_PROFILER_END();
            return;
        }
        const auto context = ContextImpl::createServiceContext(mApp, token, mApp->getMainLoop());
        service->attachBaseContext(context);
        serviceRecord = std::make_shared<ServiceClientRecord>(serviceName, service);
        mApp->addService(serviceRecord);
    }
    serviceRecord->onBind(intent, conn);
    AM_PROFILER_END();
    return;
}

void ApplicationThreadStub::onUnbindService(const sp<IBinder>& token) {
    AM_PROFILER_BEGIN();
    auto serviceRecord = mApp->findService(token);
    if (serviceRecord) {
        serviceRecord->onUnbind();
    }
    AM_PROFILER_END();
    return;
}

} // namespace app
} // namespace os
