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

#include <assert.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <fcntl.h>
#include <unistd.h>
#include <uv.h>

#include <mutex>

#include "ActivityClientRecord.h"
#include "ActivityTrace.h"
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
    Status scheduleStartActivity(const sp<IBinder>& token, const std::optional<Intent>& intent);
    Status scheduleResumeActivity(const sp<IBinder>& token, const std::optional<Intent>& intent);
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
    Status scheduleReceiveIntent(const sp<IBinder>& token, const Intent& intent);

    Status setForegroundApplication(bool isForeground);
    Status terminateApplication();

private:
    int onLaunchActivity(const string& activityName, const sp<IBinder>& token,
                         const Intent& intent);
    int onStartActivity(const sp<IBinder>& token, const std::optional<Intent>& intent);
    int onResumeActivity(const sp<IBinder>& token, const std::optional<Intent>& intent);
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

ApplicationThread::~ApplicationThread() {}

void ApplicationThread::stop() {
    ALOGD("ApplicationThread::stop");
    mApp->clearActivityAndService();
    // delay a while for receive msg from Server and other task.
    postDelayTask([this](void*) { UvLoop::stop(); }, 100);
}

static void signalHandler(uv_signal_t* handle, int signum) {
    ALOGW("warning: receive signal:%d", signum);
    ApplicationThread* appThread = static_cast<ApplicationThread*>(handle->data);
    appThread->stop();
}

int ApplicationThread::mainRun(int argc, char** argv) {
    if (argc < 2) {
        ALOGE("illegally launch Application!!!");
        return -1;
    }
    ALOGI("start Application:%s execfile:%s", argv[1], argv[0]);

    uv_signal_t sigterm;
    uv_signal_init(get(), &sigterm);
    sigterm.data = this;
    uv_signal_start(&sigterm, signalHandler, SIGTERM);

    int binderFd;
    android::IPCThreadState::self()->setupPolling(&binderFd);
    if (binderFd < 0) {
        ALOGE("failed to open binder device:%d", errno);
        return -2;
    }
    UvPoll pollBinder(get(), binderFd);
    pollBinder.start(UV_READABLE, [](int fd, int status, int events, void* data) {
        android::IPCThreadState::self()->handlePolledCommands();
    });

    android::sp<ApplicationThreadStub> appThread(new ApplicationThreadStub);
    mApp->setPackageName(argv[1]);
    mApp->onCreate(); /** Application create here */
    appThread->bind(mApp);

    ActivityManager am;
    if (0 != am.attachApplication(appThread)) {
        ALOGE("ApplicationThread attach failure");
        return -3;
    }

    run();
    pollBinder.close();
    uv_close((uv_handle_t*)&sigterm, NULL);
    // run twice to clear uv handler
    run(UV_RUN_NOWAIT);
    run(UV_RUN_NOWAIT);
    // then destory app
    mApp->onDestroy(); /** Application destroy here */

    // set uv close flag
    if (close() != 0) {
        int tryCloseCnt = 50;
#ifdef CONFIG_MM_KASAN
        tryCloseCnt = 200;
#endif
        while (isAlive() && --tryCloseCnt) {
            usleep(300000);
            run(UV_RUN_NOWAIT);
            ALOGW("uv loop run once, perform unfinished tasks");
        }
        if (close() != 0) {
            ALOGE("uv loop can't close properly, there's a memory leak!!!");
            printAllHandles();
            assert(0);
        }
    }
    ALOGW("Application[%s]:%s has been stopped!!!", argv[0], argv[1]);

    return 0;
}

Status ApplicationThreadStub::scheduleLaunchActivity(const std::string& activityName,
                                                     const sp<IBinder>& token,
                                                     const Intent& intent) {
    ALOGD("scheduleLaunchActivity package:%s activity:%s token[%p]", mApp->getPackageName().c_str(),
          activityName.c_str(), token.get());
    onLaunchActivity(activityName, token, intent);
    return Status::ok();
}

Status ApplicationThreadStub::scheduleStartActivity(const sp<IBinder>& token,
                                                    const std::optional<Intent>& intent) {
    ALOGD("scheduleStartActivity package:%s token[%p]", mApp->getPackageName().c_str(),
          token.get());
    onStartActivity(token, intent);
    return Status::ok();
}

Status ApplicationThreadStub::scheduleResumeActivity(const sp<IBinder>& token,
                                                     const std::optional<Intent>& intent) {
    ALOGD("scheduleResumeActivity package:%s token[%p]", mApp->getPackageName().c_str(),
          token.get());
    onResumeActivity(token, intent);
    return Status::ok();
}

Status ApplicationThreadStub::schedulePauseActivity(const sp<IBinder>& token) {
    ALOGD("schedulePauseActivity package:%s token[%p]", mApp->getPackageName().c_str(),
          token.get());
    onPauseActivity(token);
    return Status::ok();
}

Status ApplicationThreadStub::scheduleStopActivity(const sp<IBinder>& token) {
    ALOGD("scheduleStopActivity package:%s token[%p]", mApp->getPackageName().c_str(), token.get());
    onStopActivity(token);
    return Status::ok();
}

Status ApplicationThreadStub::scheduleDestroyActivity(const sp<IBinder>& token) {
    ALOGD("scheduleDestroyActivity package:%s token[%p]", mApp->getPackageName().c_str(),
          token.get());
    onDestroyActivity(token);
    return Status::ok();
}

Status ApplicationThreadStub::scheduleBindService(const string& serviceName,
                                                  const sp<IBinder>& token, const Intent& intent,
                                                  const sp<IServiceConnection>& conn) {
    ALOGD("scheduleBindService token[%p]", token.get());
    onBindService(serviceName, token, intent, conn);
    return Status::ok();
}

Status ApplicationThreadStub::scheduleUnbindService(const sp<IBinder>& token) {
    ALOGD("scheduleUnbindService token[%p]", token.get());
    onUnbindService(token);
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
    onStartService(serviceName, token, intent);
    return Status::ok();
}

Status ApplicationThreadStub::scheduleStopService(const sp<IBinder>& token) {
    ALOGD("scheduleStopService package:%s token[%p]", mApp->getPackageName().c_str(), token.get());
    onStopService(token);
    return Status::ok();
}

Status ApplicationThreadStub::scheduleReceiveIntent(const sp<IBinder>& token,
                                                    const Intent& intent) {
    ALOGI("scheduleReceiveIntent token[%p] intent:%s", token.get(), intent.mTarget.c_str());
    if (token == IInterface::asBinder(this)) {
        mApp->onReceiveIntent(intent);
    } else {
        if (auto activity = mApp->findActivity(token)) {
            activity->handleReceiveIntent(intent);
        } else if (auto service = mApp->findService(token)) {
            service->handleReceiveIntent(intent);
        }
    }
    return Status::ok();
}

Status ApplicationThreadStub::setForegroundApplication(bool isForeground) {
    ALOGD("setForegroundApplication package:%s %s", mApp->getPackageName().c_str(),
          isForeground ? "true" : "false");
    if (isForeground) {
        mApp->onForeground();
    } else {
        mApp->onBackground();
    }

    return Status::ok();
}

Status ApplicationThreadStub::terminateApplication() {
    ALOGW("terminateApplication package:%s", mApp->getPackageName().c_str());
    // delay clear activity for lifecycle changes
    mApp->getMainLoop()->postDelayTask(
            [this](void*) {
                mApp->clearActivityAndService();
                ALOGW("ApplicationThread stop");
                mApp->getMainLoop()->stop();
            },
            300);
    return Status::ok();
}

int ApplicationThreadStub::onLaunchActivity(const std::string& activityName,
                                            const sp<IBinder>& token, const Intent& intent) {
    AM_PROFILER_BEGIN();
    int ret = 0;
    std::shared_ptr<Activity> activity = mApp->createActivity(activityName);
    if (activity != nullptr) {
        auto context =
                ContextImpl::createActivityContext(mApp, activityName, token, mApp->getMainLoop());
        activity->attach(context);
        auto activityRecord = std::make_shared<ActivityClientRecord>(activityName, activity);
        if (activityRecord->onCreate(intent) == 0) {
            mApp->addActivity(token, activityRecord);
        } else {
            ALOGE("Activity %s/%s create failure", mApp->getPackageName().c_str(),
                  activityName.c_str());
            ret = -1;
        }
    } else {
        ALOGE("the %s/%s is not register", mApp->getPackageName().c_str(), activityName.c_str());
        ret = -2;
    }
    AM_PROFILER_END();
    return ret;
}

int ApplicationThreadStub::onStartActivity(const sp<IBinder>& token,
                                           const std::optional<Intent>& intent) {
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

int ApplicationThreadStub::onResumeActivity(const sp<IBinder>& token,
                                            const std::optional<Intent>& intent) {
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
        const auto context =
                ContextImpl::createServiceContext(mApp, serviceName, token, mApp->getMainLoop());
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
        mApp->deleteService(token);
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
        const auto context =
                ContextImpl::createServiceContext(mApp, serviceName, token, mApp->getMainLoop());
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
