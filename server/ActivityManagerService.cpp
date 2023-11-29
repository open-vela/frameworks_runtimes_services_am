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

#define LOG_TAG "AMS"

#include "am/ActivityManagerService.h"

#include <binder/IPCThreadState.h>
#include <pm/PackageManager.h>
#include <utils/Log.h>

#include <list>
#include <map>
#include <string>
#include <vector>

#include "ActivityTrace.h"
#include "AppRecord.h"
#include "AppSpawn.h"
#include "IntentAction.h"
#include "LowMemoryManager.h"
#include "ProcessPriorityPolicy.h"
#include "TaskBoard.h"
#include "TaskStackManager.h"
#include "app/ActivityManager.h"
#include "app/UvLoop.h"

namespace os {
namespace am {

using namespace std;
using namespace os::app;
using namespace os::pm;

using android::IBinder;
using android::sp;

/** Different applications have different operating environments **/
static const string APP_TYPE_QUICK = "QUICKAPP";
static const string APP_TYPE_NATIVE = "NATIVE";

// quickapp service runs in a separate process, vservice
static const string VSERVICE_EXEC_NAME = "vservice";
/******************************************************************/

static void getPackageAndComponentName(const string& target, string& packageName,
                                       string& componentName);

class ActivityManagerInner {
public:
    ActivityManagerInner(uv_loop_t* looper);

    int attachApplication(const sp<IApplicationThread>& app);
    int startActivity(const sp<IBinder>& token, const Intent& intent, int32_t requestCode);
    int stopActivity(const Intent& intent, int32_t resultCode);
    bool finishActivity(const sp<IBinder>& token, int32_t resultCode,
                        const std::optional<Intent>& resultData);
    bool moveActivityTaskToBackground(const sp<IBinder>& token, bool nonRoot);

    void reportActivityStatus(const sp<IBinder>& token, int32_t status);
    int startService(const Intent& intent);
    int stopService(const Intent& intent);
    int stopServiceByToken(const sp<IBinder>& token);
    void reportServiceStatus(const sp<IBinder>& token, int32_t status);

    int bindService(const sp<IBinder>& caller, const Intent& intent,
                    const sp<IServiceConnection>& conn);
    void unbindService(const sp<IServiceConnection>& conn);
    void publishService(const sp<IBinder>& token, const sp<IBinder>& serviceBinder);

    int32_t sendBroadcast(const Intent& intent);
    int32_t registerReceiver(const std::string& action, const sp<IBroadcastReceiver>& receiver);
    void unregisterReceiver(const sp<IBroadcastReceiver>& receiver);

    void dump(int fd, const android::Vector<android::String16>& args);

    void systemReady();
    void procAppTerminated(const std::shared_ptr<AppRecord>& appRecord);

    void setWindowManager(sp<::os::wm::IWindowManager> wm) {
        mWindowManager = wm;
    }

private:
    void stopServiceReal(ServiceHandler& service);
    int startHomeActivity();
    int getRuntimeEnvironment(const string& packagename, string& newpackage, string& newexecfile);
    int submitAppStartupTask(const string& packageName, const string& prcocessName,
                             const string& execfile, AppAttachTask::TaskFunc&& task,
                             bool isSupportMultiTask);

private:
    std::shared_ptr<UvLoop> mLooper;
    TaskBoard mPendTask;
    ServiceList mServices;
    AppInfoList mAppInfo;
    TaskStackManager mTaskManager;
    IntentAction mActionFilter;
    PackageManager mPm;
    sp<::os::wm::IWindowManager> mWindowManager;
    map<string, list<sp<IBroadcastReceiver>>> mReceivers; /** Broadcast */
    LowMemoryManager mLmk;
    ProcessPriorityPolicy mPriorityPolicy;
};

ActivityManagerInner::ActivityManagerInner(uv_loop_t* looper)
      : mTaskManager(mPendTask), mPriorityPolicy(&mLmk) {
    mLooper = std::make_shared<UvLoop>(looper);
    mPendTask.attachLoop(mLooper);
    mLmk.init(mLooper);
    mLmk.setLMKExecutor([this](pid_t pid) {
        if (auto apprecord = mAppInfo.findAppInfo(pid)) {
            ALOGW("LMK stop application:%s", apprecord->mPackageName.c_str());
            apprecord->stopApplication();
        }
    });
}

int ActivityManagerInner::attachApplication(const sp<IApplicationThread>& app) {
    AM_PROFILER_BEGIN();
    const int callerPid = android::IPCThreadState::self()->getCallingPid();
    auto appRecord = mAppInfo.findAppInfo(callerPid);
    ALOGI("attachApplication. pid:%d appRecord:[%p]", callerPid, appRecord.get());
    if (appRecord) {
        ALOGE("the application:%s had be attached", appRecord->mPackageName.c_str());
        AM_PROFILER_END();
        return android::BAD_VALUE;
    }

    const int callerUid = android::IPCThreadState::self()->getCallingUid();

    string packageName;
    if (mAppInfo.getAttachingAppName(callerPid, packageName)) {
        appRecord = std::make_shared<AppRecord>(app, packageName, callerPid, callerUid, &mAppInfo,
                                                &mPriorityPolicy);
        mAppInfo.addAppInfo(appRecord);
        const AppAttachTask::Event event(callerPid, appRecord);
        mPendTask.eventTrigger(event);
    } else {
        ALOGE("the application:%d attaching is illegally", callerPid);
    }

    AM_PROFILER_END();
    return android::OK;
}

int ActivityManagerInner::startActivity(const sp<IBinder>& caller, const Intent& intent,
                                        int32_t requestCode) {
    AM_PROFILER_BEGIN();

    if (mTaskManager.getActiveTask()->getTopActivity()->getStatus() != ActivityRecord::RESUMED) {
        ALOGE("The top Activity status is not ready, Please wait a moment before requesting again");
        AM_PROFILER_END();
        return android::INVALID_OPERATION;
    }

    string activityTarget;
    if (!intent.mTarget.empty()) {
        activityTarget = intent.mTarget;
    } else {
        if (!mActionFilter.getFirstTargetByAction(intent.mAction, activityTarget)) {
            ALOGE("can't find the Activity by action:%s", intent.mAction.c_str());
            AM_PROFILER_END();
            return android::BAD_VALUE;
        }
    }
    ALOGI("start activity:%s intent:%s %s flag:%d", activityTarget.c_str(), intent.mAction.c_str(),
          intent.mData.c_str(), intent.mFlag);

    /** get Package and Activity info for PMS */
    string packageName;
    string activityName;
    string taskAffinity;
    ActivityRecord::LaunchMode launchMode;
    getPackageAndComponentName(activityTarget, packageName, activityName);

    PackageInfo packageInfo;
    if (mPm.getPackageInfo(packageName, &packageInfo)) {
        ALOGE("packagename:%s is not installed!!!", packageName.c_str());
        AM_PROFILER_END();
        return android::BAD_VALUE;
    }

    /** ready to start Activity */
    if (activityName.empty()) {
        auto appTask = mTaskManager.findTask(packageName);
        if (appTask) {
            mTaskManager.switchTaskToActive(appTask, intent);
            AM_PROFILER_END();
            return android::OK;
        } else {
            activityName = packageInfo.entry;
        }
    }

    /** We need to check that the Intent.flag makes sense and perhaps modify it */
    int startFlag = intent.mFlag;

    for (const auto& it : packageInfo.activitiesInfo) {
        if (it.name == activityName) {
            launchMode = ActivityRecord::launchModeToInt(it.launchMode);
            taskAffinity = it.taskAffinity;
        }
    }
    /** Entry Activity taskAffinity can only be packagename */
    if (activityName == packageInfo.entry) {
        taskAffinity = packageName;
        /** The entry activity must be taskTag for the taskStack, so we need it in another task
         */
        startFlag |= Intent::FLAG_ACTIVITY_NEW_TASK;
        if (launchMode != ActivityRecord::SINGLE_TASK) {
            /** Entry Activity only support singleTask or singleInstance */
            launchMode = ActivityRecord::SINGLE_INSTANCE;
        }
    }

    auto callActivity = mTaskManager.getActivity(caller);
    if (!callActivity || callActivity->getLaunchMode() == ActivityRecord::SINGLE_INSTANCE ||
        launchMode == ActivityRecord::SINGLE_INSTANCE) {
        /** if the caller's(maybe Service) Stack does't exist, we must new a task */
        startFlag |= Intent::FLAG_ACTIVITY_NEW_TASK;
    }

    ActivityStackHandler targetTask;
    bool isNewTask = false;
    if (startFlag & Intent::FLAG_ACTIVITY_NEW_TASK) {
        targetTask = mTaskManager.findTask(taskAffinity);
        if (!targetTask) {
            targetTask = std::make_shared<ActivityStack>(taskAffinity);
            isNewTask = true;
        }
    } else {
        targetTask = mTaskManager.getActiveTask();
    }

    ActivityHandler targetActivity;
    if (!isNewTask) {
        switch (launchMode) {
            case ActivityRecord::SINGLE_TOP:
                startFlag |= Intent::FLAG_ACTIVITY_SINGLE_TOP;
                if (targetTask->getTopActivity()->getName() == (packageName + "/" + activityName)) {
                    targetActivity = targetTask->getTopActivity();
                }
                break;
            case ActivityRecord::SINGLE_TASK:
            case ActivityRecord::SINGLE_INSTANCE:
                startFlag |= Intent::FLAG_ACTIVITY_CLEAR_TOP;
                targetActivity = targetTask->findActivity(packageName + "/" + activityName);
                break;
            default:
                break;
        }
    }

    if (!targetActivity) {
        auto newActivity = std::make_shared<ActivityRecord>(packageName + "/" + activityName,
                                                            caller, requestCode, launchMode,
                                                            targetTask, intent, mWindowManager);
        const auto appInfo = mAppInfo.findAppInfoWithAlive(packageName);
        if (appInfo) {
            newActivity->setAppThread(appInfo);
            mTaskManager.pushNewActivity(targetTask, newActivity, startFlag);
        } else {
            const auto task = [this, targetTask, newActivity,
                               startFlag](const AppAttachTask::Event* e) {
                mPriorityPolicy.add(e->mPid, true);
                newActivity->setAppThread(e->mAppRecord);
                mTaskManager.pushNewActivity(targetTask, newActivity, startFlag);
            };
            if (submitAppStartupTask(packageName, packageName, packageInfo.execfile,
                                     std::move(task), false) != 0) {
                ALOGW("submitAppStartupTask failure");
                AM_PROFILER_END();
                return android::INVALID_OPERATION;
            }
        }
    } else {
        /** if there is no need to create an Activity, caller/requestCode is invalid */
        mTaskManager.turnToActivity(targetTask, targetActivity, intent, startFlag);
    }

    AM_PROFILER_END();
    return android::OK;
}

int ActivityManagerInner::stopActivity(const Intent& intent, int32_t resultCode) {
    AM_PROFILER_BEGIN();
    ALOGI("stopActivity, target:%s", intent.mTarget.c_str());
    int ret = android::OK;

    if (intent.mTarget.empty()) {
        ALOGW("stopActivity: The target is null");
        ret = android::BAD_VALUE;
    } else {
        string packageName;
        string activityName;
        getPackageAndComponentName(intent.mTarget, packageName, activityName);

        ActivityHandler activity;
        auto appinfo = mAppInfo.findAppInfoWithAlive(packageName);
        if (appinfo) {
            if (activityName.empty()) {
                appinfo->stopApplication();
            } else {
                activity = appinfo->checkActivity(intent.mTarget);
                if (activity) {
                    const auto callActivity = mTaskManager.getActivity(activity->getCaller());
                    if (activity->getRequestCode() != ActivityManager::NO_REQUEST && callActivity) {
                        callActivity->onResult(activity->getRequestCode(), resultCode, intent);
                    }
                    mTaskManager.finishActivity(activity);
                } else {
                    ALOGW("The Activity:%s does not exist!", intent.mTarget.c_str());
                    ret = android::BAD_VALUE;
                }
            }
        }
    }

    AM_PROFILER_END();
    return ret;
}

bool ActivityManagerInner::finishActivity(const sp<IBinder>& token, int32_t resultCode,
                                          const std::optional<Intent>& resultData) {
    AM_PROFILER_BEGIN();
    auto activity = mTaskManager.getActivity(token);
    if (!activity) {
        ALOGE("finishActivity: The token is invalid");
        AM_PROFILER_END();
        return false;
    }
    ALOGI("finishActivity called by %s", activity->getName().c_str());

    /** when last pasued, then set result to next */
    const auto callActivity = mTaskManager.getActivity(activity->getCaller());
    if (activity->getRequestCode() != ActivityManager::NO_REQUEST && resultData.has_value() &&
        callActivity) {
        callActivity->onResult(activity->getRequestCode(), resultCode, resultData.value());
    }

    mTaskManager.finishActivity(activity);

    AM_PROFILER_END();
    return true;
}

bool ActivityManagerInner::moveActivityTaskToBackground(const sp<IBinder>& token, bool nonRoot) {
    AM_PROFILER_BEGIN();
    bool ret = false;
    auto activity = mTaskManager.getActivity(token);
    if (activity) {
        ALOGI("moveActivityTaskToBackground, activity:%s nonRoot:%s", activity->getName().c_str(),
              nonRoot ? "true" : "false");
        auto activityTask = activity->getTask();
        if (activityTask && (nonRoot || activityTask->getRootActivity() == activity)) {
            ret = mTaskManager.moveTaskToBackground(activityTask);
        }
    } else {
        ALOGE("moveActivityTaskToBackground: The token is invalid");
    }

    AM_PROFILER_END();
    return ret;
}

void ActivityManagerInner::reportActivityStatus(const sp<IBinder>& token, int32_t status) {
    AM_PROFILER_BEGIN();
    ALOGI("reportActivityStatus called by %s [%s]",
          mTaskManager.getActivity(token)->getName().c_str(), ActivityRecord::statusToStr(status));

    const ActivityLifeCycleTask::Event event((ActivityRecord::Status)status, token);
    mPendTask.eventTrigger(event);

    // Only "resume" and "destroy" need special process.
    if (status == ActivityRecord::RESUMED) {
        const ActivityWaitResume::Event event2(token);
        mPendTask.eventTrigger(event2);
    } else if (status == ActivityRecord::DESTROYED) {
        auto activity = mTaskManager.getActivity(token);
        activity->setStatus(ActivityRecord::DESTROYED);
        mTaskManager.deleteActivity(activity);
        if (const auto appRecord = activity->getAppRecord()) {
            if (!appRecord->checkActiveStatus()) {
                appRecord->stopApplication();
            }
        }
    }

    AM_PROFILER_END();
    return;
}

int ActivityManagerInner::startService(const Intent& intent) {
    AM_PROFILER_BEGIN();
    string packageName;
    string serviceName;
    if (intent.mTarget.empty()) {
        string target;
        mActionFilter.getFirstTargetByAction(intent.mAction, target);
        getPackageAndComponentName(target, packageName, serviceName);
    } else {
        getPackageAndComponentName(intent.mTarget, packageName, serviceName);
    }

    if (serviceName.empty()) {
        ALOGW("startService: incorrect intents[%s %s], can't find target service",
              intent.mTarget.c_str(), intent.mAction.c_str());
        AM_PROFILER_END();
        return android::BAD_VALUE;
    }
    ALOGI("start service:%s/%s", packageName.c_str(), serviceName.c_str());

    string servicePackageName;
    string execfile;
    if (getRuntimeEnvironment(packageName, servicePackageName, execfile) != 0) {
        AM_PROFILER_END();
        return android::BAD_VALUE;
    }

    ServiceHandler service = mServices.findService(servicePackageName, serviceName);
    if (service) {
        service->start(intent);
    } else {
        std::shared_ptr<AppRecord> appRecord;
        appRecord = mAppInfo.findAppInfoWithAlive(packageName);
        if (appRecord) {
            const sp<IBinder> token(new android::BBinder());
            service = std::make_shared<ServiceRecord>(serviceName, token, appRecord);
            mServices.addService(service);
            service->start(intent);
        } else {
            const auto task = [this, serviceName, intent](const AppAttachTask::Event* e) {
                const sp<IBinder> token(new android::BBinder());
                auto serviceRecord =
                        std::make_shared<ServiceRecord>(serviceName, token, e->mAppRecord);
                mPriorityPolicy.add(e->mPid, false);
                mServices.addService(serviceRecord);
                serviceRecord->start(intent);
            };
            if (submitAppStartupTask(packageName, servicePackageName, execfile, std::move(task),
                                     true) != 0) {
                ALOGW("submitAppStartupTask failure");
                AM_PROFILER_END();
                return android::INVALID_OPERATION;
            }
        }
    }
    AM_PROFILER_END();
    return android::OK;
}

int ActivityManagerInner::stopService(const Intent& intent) {
    AM_PROFILER_BEGIN();
    string packageName;
    string serviceName;
    if (intent.mTarget.empty()) {
        string target;
        mActionFilter.getFirstTargetByAction(intent.mAction, target);
        getPackageAndComponentName(target, packageName, serviceName);
    } else {
        getPackageAndComponentName(intent.mTarget, packageName, serviceName);
    }

    ALOGI("stop service:%s/%s", packageName.c_str(), serviceName.c_str());

    string servicePackageName;
    string execfile;
    if (getRuntimeEnvironment(packageName, servicePackageName, execfile) != 0) {
        AM_PROFILER_END();
        return android::BAD_VALUE;
    }

    ServiceHandler service = mServices.findService(servicePackageName, serviceName);
    if (!service) {
        ALOGW("the Service:%s is not running", serviceName.c_str());
        AM_PROFILER_END();
        return android::DEAD_OBJECT;
    }

    stopServiceReal(service);
    AM_PROFILER_END();
    return 0;
}

int ActivityManagerInner::bindService(const sp<IBinder>& caller, const Intent& intent,
                                      const sp<IServiceConnection>& conn) {
    AM_PROFILER_BEGIN();
    string packageName;
    string serviceName;
    if (intent.mTarget.empty()) {
        string target;
        mActionFilter.getFirstTargetByAction(intent.mAction, target);
        getPackageAndComponentName(target, packageName, serviceName);
    } else {
        getPackageAndComponentName(intent.mTarget, packageName, serviceName);
    }

    ALOGI("bind service:%s/%s connection[%p]", packageName.c_str(), serviceName.c_str(),
          conn.get());

    string servicePackageName;
    string execfile;
    if (getRuntimeEnvironment(packageName, servicePackageName, execfile) != 0) {
        AM_PROFILER_END();
        return android::BAD_VALUE;
    }

    ServiceHandler service = mServices.findService(servicePackageName, serviceName);
    if (!service) {
        const auto appRecord = mAppInfo.findAppInfoWithAlive(servicePackageName);
        if (appRecord) {
            const sp<IBinder> token(new android::BBinder());
            service = std::make_shared<ServiceRecord>(serviceName, token, appRecord);
            mServices.addService(service);
            service->bind(caller, conn, intent);
        } else {
            const auto task = [this, serviceName, caller, conn,
                               intent](const AppAttachTask::Event* e) {
                const sp<IBinder> token(new android::BBinder());
                auto serviceRecord =
                        std::make_shared<ServiceRecord>(serviceName, token, e->mAppRecord);
                mPriorityPolicy.add(e->mPid, false);
                mServices.addService(serviceRecord);
                serviceRecord->bind(caller, conn, intent);
            };

            if (submitAppStartupTask(packageName, servicePackageName, execfile, std::move(task),
                                     true) != 0) {
                ALOGW("submitAppStartupTask failure");
                AM_PROFILER_END();
                return android::INVALID_OPERATION;
            }
        }
    } else {
        service->bind(caller, conn, intent);
    }
    AM_PROFILER_END();
    return 0;
}

void ActivityManagerInner::unbindService(const sp<IServiceConnection>& conn) {
    AM_PROFILER_BEGIN();
    ALOGI("unbindService connection[%p]", conn.get());
    mServices.unbindConnection(conn);
    AM_PROFILER_END();
    return;
}

void ActivityManagerInner::publishService(const sp<IBinder>& token,
                                          const sp<IBinder>& serviceBinder) {
    AM_PROFILER_BEGIN();
    ALOGI("publishService service[%p]", token.get());
    auto service = mServices.getService(token);
    if (service) {
        service->mServiceBinder = serviceBinder;
    } else {
        ALOGE("publishService error. the Service token[%p] does not exist", token.get());
    }
    AM_PROFILER_END();
}

int ActivityManagerInner::stopServiceByToken(const sp<IBinder>& token) {
    AM_PROFILER_BEGIN();
    auto service = mServices.getService(token);
    if (!service) {
        ALOGW("unbelievable! Can't get record when service stop self:%s/%s",
              service->getPackageName()->c_str(), service->mServiceName.c_str());
        AM_PROFILER_END();
        return android::DEAD_OBJECT;
    }
    ALOGI("stopServiceByToken. %s/%s", service->getPackageName()->c_str(),
          service->mServiceName.c_str());
    stopServiceReal(service);
    AM_PROFILER_END();
    return 0;
}

void ActivityManagerInner::stopServiceReal(ServiceHandler& service) {
    ALOGD("stopService %s/%s", service->getPackageName()->c_str(), service->mServiceName.c_str());
    if (service->mStatus < ServiceRecord::DESTROYING) {
        service->stop();
    }
}

void ActivityManagerInner::reportServiceStatus(const sp<IBinder>& token, int32_t status) {
    AM_PROFILER_BEGIN();
    auto service = mServices.getService(token);
    if (!service) {
        ALOGE("service is not exist");
        AM_PROFILER_END();
        return;
    }
    ALOGI("reportServiceStatus %s/%s status:%s->%s", service->getPackageName()->c_str(),
          service->mServiceName.c_str(), ServiceRecord::statusToStr(service->mStatus),
          ServiceRecord::statusToStr(status));

    switch (status) {
        case ServiceRecord::CREATED:
        case ServiceRecord::STARTED:
        case ServiceRecord::BINDED:
            break;
        case ServiceRecord::UNBINDED: {
            if (!service->isAlive()) service->stop();
            break;
        }
        case ServiceRecord::DESTROYED: {
            mServices.deleteService(token);
            if (auto appRecord = service->mApp.lock()) {
                if (!appRecord->checkActiveStatus()) {
                    appRecord->stopApplication();
                }
            }
            break;
        }
        default: {
            ALOGE("unbeliveable!!! service status:%d is illegal", status);
            AM_PROFILER_END();
            return;
        }
    }
    service->mStatus = status;
    const ServiceReportStatusTask::Event event(status, token);
    mPendTask.eventTrigger(event);
    AM_PROFILER_END();
}

int32_t ActivityManagerInner::sendBroadcast(const Intent& intent) {
    ALOGI("sendBroadcast:%s", intent.mAction.c_str());
    auto receivers = mReceivers.find(intent.mAction);
    if (receivers != mReceivers.end()) {
        for (auto& receiver : receivers->second) {
            receiver->receiveBroadcast(intent);
        }
    }
    return 0;
}

int32_t ActivityManagerInner::registerReceiver(const std::string& action,
                                               const sp<IBroadcastReceiver>& receiver) {
    ALOGI("registerReceiver:%s", action.c_str());
    auto receivers = mReceivers.find(action);
    if (receivers != mReceivers.end()) {
        receivers->second.emplace_back(receiver);
        ALOGI("register success, cnt:%d", receivers->second.size());
    } else {
        std::list<sp<IBroadcastReceiver>> receiverList;
        receiverList.emplace_back(receiver);
        mReceivers.emplace(action, std::move(receiverList));
        ALOGI("add new receiver success");
    }
    return 0;
}

void ActivityManagerInner::unregisterReceiver(const sp<IBroadcastReceiver>& receiver) {
    ALOGI("unregisterReceiver");
    for (auto& pair : mReceivers) {
        for (auto it = pair.second.begin(); it != pair.second.end(); ++it) {
            if (android::IInterface::asBinder(*it) == android::IInterface::asBinder(receiver)) {
                pair.second.erase(it);
                break;
            }
        }
    }
}

void ActivityManagerInner::systemReady() {
    AM_PROFILER_BEGIN();
    ALOGD("### systemReady ### ");
    AppSpawn::signalInit([this](int pid) {
        ALOGW("AppSpawn pid:%d had exit", pid);
        auto app = mAppInfo.findAppInfo(pid);
        if (app) {
            procAppTerminated(app);
            mAppInfo.deleteAppInfo(pid);
            mPriorityPolicy.remove(pid);
        }
    });

    vector<PackageInfo> allPackageInfo;
    if (mPm.getAllPackageInfo(&allPackageInfo) == 0) {
        for (auto item : allPackageInfo) {
            for (auto activity : item.activitiesInfo) {
                for (auto action : activity.actions) {
                    /** make action <----> packagename/activityname */
                    mActionFilter.setIntentAction(action, item.packageName + "/" + activity.name);
                }
            }
        }
    }

    startHomeActivity();
    AM_PROFILER_END();
    return;
}

void ActivityManagerInner::procAppTerminated(const std::shared_ptr<AppRecord>& appRecord) {
    /** All activity needs to be destroyed from the stack */
    std::vector<std::weak_ptr<ActivityRecord>> needDeleteActivity;
    needDeleteActivity.swap(appRecord->mExistActivity);
    /** First mark all Destroy Activity. */
    for (auto& it : needDeleteActivity) {
        if (auto activityRecord = it.lock()) {
            activityRecord->abnormalExit();
        }
    }
    /** Remove from task stack */
    for (auto& it : needDeleteActivity) {
        if (auto activityRecord = it.lock()) {
            mTaskManager.deleteActivity(activityRecord);
        }
    }
    needDeleteActivity.clear();

    std::vector<std::weak_ptr<ServiceRecord>> needDeleteService;
    needDeleteService.swap(appRecord->mExistService);
    for (auto& it : needDeleteService) {
        if (auto serviceRecord = it.lock()) {
            serviceRecord->abnormalExit();
            mServices.deleteService(serviceRecord->mToken);
        }
    }
    needDeleteService.clear();
}

void ActivityManagerInner::dump(int fd, const android::Vector<android::String16>& args) {
    std::ostringstream os;
    os << mTaskManager << mPriorityPolicy;
    write(fd, os.str().c_str(), os.str().size());
}

int ActivityManagerInner::startHomeActivity() {
    /** start the launch app */
    string target;
    if (mActionFilter.getFirstTargetByAction(Intent::ACTION_HOME, target)) {
        /** find the Home App package */
        auto pos = target.find_first_of('/');
        const string packageName = target.substr(0, pos);
        ALOGI("start Home Applicaion:%s", packageName.c_str());
        PackageInfo homeApp;
        mPm.getPackageInfo(packageName, &homeApp);
        const string activityName =
                pos != string::npos ? target.substr(pos + 1, string::npos) : homeApp.entry;
        ActivityInfo entryActivity;
        for (auto a : homeApp.activitiesInfo) {
            if (a.name == activityName) {
                entryActivity = a;
            }
        }
        /** init Home ActivityStack task, the Home Activity will be pushed to this task */
        const auto homeTask = std::make_shared<ActivityStack>(entryActivity.taskAffinity);
        auto newActivity =
                std::make_shared<ActivityRecord>(packageName + "/" + activityName, nullptr,
                                                 (int32_t)ActivityManager::NO_REQUEST,
                                                 ActivityRecord::SINGLE_TASK, homeTask, Intent(),
                                                 mWindowManager);

        auto task = [this, homeTask, newActivity](const AppAttachTask::Event* e) {
            mPriorityPolicy.add(e->mPid, true);
            newActivity->setAppThread(e->mAppRecord);
            this->mTaskManager.initHomeTask(homeTask, newActivity);
        };
        if (submitAppStartupTask(packageName, packageName, homeApp.execfile.c_str(),
                                 std::move(task), false) != 0) {
            ALOGE("Startup home app failure!!!");
            AM_PROFILER_END();
            return android::INVALID_OPERATION;
        }
    } else {
        ALOGE("systemReady error: can't launch Home Activity");
        return android::NAME_NOT_FOUND;
    }

    return android::OK;
}

int ActivityManagerInner::getRuntimeEnvironment(const string& packagename, string& newpackage,
                                                string& newexecfile) {
    PackageInfo packageInfo;
    if (mPm.getPackageInfo(packagename, &packageInfo) != 0) {
        ALOGE("error packagename:%s", packagename.c_str());
        return -1;
    }
    if (packageInfo.appType == APP_TYPE_QUICK) {
        // bad demand for quick service...
        newpackage = VSERVICE_EXEC_NAME + ':' + packageInfo.packageName;
        newexecfile = VSERVICE_EXEC_NAME;
    } else {
        newpackage = packageInfo.packageName;
        newexecfile = packageInfo.execfile;
    }
    return 0;
}

int ActivityManagerInner::submitAppStartupTask(const string& packageName,
                                               const string& prcocessName, const string& execfile,
                                               AppAttachTask::TaskFunc&& task,
                                               bool isSupportMultiTask) {
    int pid = mAppInfo.getAttachingAppPid(prcocessName);
    if (pid < 0) {
        pid = AppSpawn::appSpawn(execfile.c_str(), {packageName});
        if (pid > 0) {
            mAppInfo.addAppWaitingAttach(prcocessName, pid);
        } else {
            ALOGE("appSpawn App:%s error", execfile.c_str());
            return -1;
        }
    } else if (!isSupportMultiTask) {
        ALOGW("the Application:%s[%d] is waitting for attach, please wait a moment before "
              "requesting again",
              packageName.c_str(), pid);
        return -1;
    }

    mPendTask.commitTask(std::make_shared<AppAttachTask>(pid, task));
    return 0;
}

void getPackageAndComponentName(const string& target, string& packageName, string& componentName) {
    const auto pos = target.find_first_of('/');
    packageName = std::move(target.substr(0, pos));
    componentName =
            std::move(pos == std::string::npos ? "" : target.substr(pos + 1, std::string::npos));
}

ActivityManagerService::ActivityManagerService(uv_loop_t* looper) {
    mInner = new ActivityManagerInner(looper);
}

ActivityManagerService::~ActivityManagerService() {
    delete mInner;
}

Status ActivityManagerService::attachApplication(const sp<IApplicationThread>& app, int32_t* ret) {
    *ret = mInner->attachApplication(app);
    return Status::ok();
}

Status ActivityManagerService::startActivity(const sp<IBinder>& token, const Intent& intent,
                                             int32_t requestCode, int32_t* ret) {
    *ret = mInner->startActivity(token, intent, requestCode);
    return Status::ok();
}

Status ActivityManagerService::stopActivity(const Intent& intent, int32_t resultCode,
                                            int32_t* ret) {
    *ret = mInner->stopActivity(intent, resultCode);
    return Status::ok();
}

Status ActivityManagerService::finishActivity(const sp<IBinder>& token, int32_t resultCode,
                                              const std::optional<Intent>& resultData, bool* ret) {
    *ret = mInner->finishActivity(token, resultCode, resultData);
    return Status::ok();
}

Status ActivityManagerService::moveActivityTaskToBackground(const sp<IBinder>& token, bool nonRoot,
                                                            bool* ret) {
    *ret = mInner->moveActivityTaskToBackground(token, nonRoot);
    return Status::ok();
}

Status ActivityManagerService::reportActivityStatus(const sp<IBinder>& token, int32_t status) {
    mInner->reportActivityStatus(token, status);
    return Status::ok();
}

Status ActivityManagerService::startService(const Intent& intent, int32_t* ret) {
    *ret = mInner->startService(intent);
    return Status::ok();
}

Status ActivityManagerService::stopService(const Intent& intent, int32_t* ret) {
    *ret = mInner->stopService(intent);
    return Status::ok();
}

Status ActivityManagerService::stopServiceByToken(const sp<IBinder>& token, int32_t* ret) {
    *ret = mInner->stopServiceByToken(token);
    return Status::ok();
}

Status ActivityManagerService::reportServiceStatus(const sp<IBinder>& token, int32_t status) {
    mInner->reportServiceStatus(token, status);
    return Status::ok();
}

Status ActivityManagerService::bindService(const sp<IBinder>& token, const Intent& intent,
                                           const sp<IServiceConnection>& conn, int32_t* ret) {
    *ret = mInner->bindService(token, intent, conn);
    return Status::ok();
}

Status ActivityManagerService::unbindService(const sp<IServiceConnection>& conn) {
    mInner->unbindService(conn);
    return Status::ok();
}

Status ActivityManagerService::publishService(const sp<IBinder>& token,
                                              const sp<IBinder>& service) {
    mInner->publishService(token, service);
    return Status::ok();
}

Status ActivityManagerService::sendBroadcast(const Intent& intent, int32_t* ret) {
    *ret = mInner->sendBroadcast(intent);
    return Status::ok();
}

Status ActivityManagerService::registerReceiver(const std::string& action,
                                                const sp<IBroadcastReceiver>& receiver,
                                                int32_t* ret) {
    *ret = mInner->registerReceiver(action, receiver);
    return Status::ok();
}

Status ActivityManagerService::unregisterReceiver(const sp<IBroadcastReceiver>& receiver) {
    mInner->unregisterReceiver(receiver);
    return Status::ok();
}

android::status_t ActivityManagerService::dump(int fd,
                                               const android::Vector<android::String16>& args) {
    mInner->dump(fd, args);
    return 0;
}

/** The service is ready to start and the application can be launched */
void ActivityManagerService::systemReady() {
    mInner->systemReady();
}

void ActivityManagerService::setWindowManager(sp<::os::wm::IWindowManager> wm) {
    mInner->setWindowManager(wm);
}

} // namespace am
} // namespace os
