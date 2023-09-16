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
#include <os/pm/PackageInfo.h>
#include <pm/PackageManager.h>
#include <utils/Log.h>

#include <list>
#include <map>
#include <string>
#include <vector>

#include "ActivityStack.h"
#include "ActivityTrace.h"
#include "AppRecord.h"
#include "AppSpawn.h"
#include "IntentAction.h"
#include "TaskBoard.h"
#include "app/ActivityManager.h"

namespace os {
namespace am {

using namespace std;
using namespace os::app;
using namespace os::pm;

using android::IBinder;
using android::sp;

void getPackageAndComponentName(const string& target, string& packageName, string& componentName);

class ActivityManagerInner {
public:
    int attachApplication(const sp<IApplicationThread>& app);
    int startActivity(const sp<IBinder>& token, const Intent& intent, int32_t requestCode);
    bool finishActivity(const sp<IBinder>& token, int32_t resultCode,
                        const std::optional<Intent>& resultData);
    void reportActivityStatus(const sp<IBinder>& token, int32_t status);
    int startService(const Intent& intent);
    int stopService(const Intent& intent);
    int stopServiceToken(const sp<IBinder>& token);
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
    ActivityHandler getActivityRecord(const sp<IBinder>& token);
    ActivityHandler startActivityReal(const std::shared_ptr<AppRecord>& app,
                                      const ActivityInfo& activityName, const Intent& intent,
                                      const sp<IBinder>& caller, const int32_t requestCode);
    void stopServiceReal(ServiceHandler& service);
    int startHomeActivity();

private:
    map<sp<IBinder>, ActivityHandler> mActivityMap;
    ServiceList mServices;
    AppInfoList mAppInfo;
    TaskStackManager mTaskManager;
    IntentAction mActionFilter;
    TaskBoard mPendTask;
    PackageManager mPm;
    sp<::os::wm::IWindowManager> mWindowManager;
    map<string, list<sp<IBroadcastReceiver>>> mReceivers; /** Broadcast */
};

int ActivityManagerInner::attachApplication(const sp<IApplicationThread>& app) {
    AM_PROFILER_BEGIN();
    const int callerPid = android::IPCThreadState::self()->getCallingPid();
    const auto appRecord = mAppInfo.findAppInfo(callerPid);
    ALOGD("attachApplication. pid:%d appRecord:[%p]", callerPid, appRecord.get());
    if (appRecord) {
        ALOGE("the application:%s had be attached", appRecord->mPackageName.c_str());
        AM_PROFILER_END();
        return android::BAD_VALUE;
    }

    const int callerUid = android::IPCThreadState::self()->getCallingUid();
    const AppAttachTask::Event event(callerPid, callerUid, app);
    mPendTask.eventTrigger(event);
    AM_PROFILER_END();
    return android::OK;
}

int ActivityManagerInner::startActivity(const sp<IBinder>& caller, const Intent& intent,
                                        int32_t requestCode) {
    AM_PROFILER_BEGIN();
    auto topActivity = mTaskManager.getActiveTask()->getTopActivity();
    if (topActivity->mStatus < ActivityRecord::STARTED ||
        topActivity->mStatus > ActivityRecord::PAUSING) {
        /** The currently active Activity is undergoing a state transition, it is't accepting
         * other startActivity requests */
        ALOGW("the top Activity:(%s) Status(%d) is changing", topActivity->mActivityName.c_str(),
              topActivity->mStatus);
        AM_PROFILER_END();
        return android::INVALID_OPERATION;
    }

    if (caller != topActivity->mToken && !(intent.mFlag & Intent::FLAG_ACTIVITY_NEW_TASK)) {
        /** if the current caller is not the top Activity */
        ALOGW("Inappropriate, the caller must startActivity in other task");
        AM_PROFILER_END();
        return android::PERMISSION_DENIED;
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
    getPackageAndComponentName(intent.mTarget, packageName, activityName);
    PackageInfo packageInfo;
    if (mPm.getPackageInfo(packageName, &packageInfo)) {
        ALOGE("error packagename:%s", packageName.c_str());
        AM_PROFILER_END();
        return android::BAD_VALUE;
    }

    bool isStartApp = false;
    if (activityName.empty()) {
        isStartApp = true;
        activityName = packageInfo.entry;
    }

    ActivityInfo activityInfo;
    for (auto it : packageInfo.activitiesInfo) {
        if (it.name == activityName) {
            activityInfo = it;
        }
    }

    if (topActivity != nullptr) {
        topActivity->pause(); /** pause windows interactive*/
    }

    /** ready to start Activity */
    auto appInfo = mAppInfo.findAppInfo(packageName);
    if (appInfo) {
        ActivityHandler activityRecord;
        if (isStartApp) {
            /** if the target app task is already launched, just switch */
            auto targetTask = mTaskManager.findTask(packageName);
            mTaskManager.switchTaskToActive(targetTask);
            activityRecord = mTaskManager.getActiveTask()->getTopActivity();
            activityRecord->resume();
        } else {
            /** get Application thread */
            activityRecord = startActivityReal(appInfo, activityInfo, intent, caller, requestCode);
        }

        /** When the target Activity has been resumed. then stop the previous Activity */
        const auto task =
                std::make_shared<ActivityReportStatusTask>(ActivityRecord::RESUMED,
                                                           activityRecord->mToken,
                                                           [this, topActivity]() {
                                                               if (mTaskManager.getActiveTask()
                                                                           ->getTopActivity() !=
                                                                   topActivity)
                                                                   topActivity->stop();
                                                           });
        mPendTask.commitTask(task);
    } else {
        /** The app hasn't started yet */
        const int pid = AppSpawn::appSpawn(packageInfo.execfile.c_str(), {packageName});
        if (pid > 0) {
            const auto waitAppAttach = [this, packageName, activityInfo, intent, caller,
                                        requestCode, topActivity](const AppAttachTask::Event* e) {
                auto appRecord =
                        std::make_shared<AppRecord>(e->mAppHandler, packageName, e->mPid, e->mUid);
                this->mAppInfo.addAppInfo(appRecord);
                auto record = this->startActivityReal(appRecord, activityInfo, intent, caller,
                                                      requestCode);
                const auto stopLastActivity =
                        std::make_shared<ActivityReportStatusTask>(ActivityRecord::RESUMED,
                                                                   record->mToken,
                                                                   [this, topActivity]() {
                                                                       topActivity->stop();
                                                                   });
                mPendTask.commitTask(stopLastActivity);
            };
            mPendTask.commitTask(std::make_shared<AppAttachTask>(pid, waitAppAttach));
        } else {
            /** Failed to start, restore the previous pause activity */
            topActivity->resume();
        }
    }
    AM_PROFILER_END();
    return android::OK;
}

ActivityHandler ActivityManagerInner::startActivityReal(const std::shared_ptr<AppRecord>& app,
                                                        const ActivityInfo& activityInfo,
                                                        const Intent& intent,
                                                        const sp<IBinder>& caller,
                                                        const int32_t requestCode) {
    TaskHandler targetTask;
    auto callerActivity = getActivityRecord(caller);

    /** check if into another taskStack */
    const string& launchMode = activityInfo.launchMode;
    bool isCreateTask = false;
    if (intent.mFlag & Intent::FLAG_ACTIVITY_NEW_TASK || launchMode == "singleTask" ||
        launchMode == "singleInstance" || callerActivity->mLaunchMode == "singleInstance") {
        targetTask = mTaskManager.findTask(activityInfo.taskAffinity);
        if (!targetTask) {
            isCreateTask = true;
            targetTask = std::make_shared<ActivityStack>(activityInfo.taskAffinity);
        }
    } else {
        targetTask = mTaskManager.getActiveTask();
    }

    ALOGD("start activity:%s launchMode:%s flag:%d", activityInfo.name.c_str(), launchMode.c_str(),
          intent.mFlag);
    bool isCreateActivity = true;
    ActivityHandler record;
    if (!isCreateTask) {
        if (intent.mFlag == Intent::NO_FLAG || intent.mFlag == Intent::FLAG_ACTIVITY_NEW_TASK) {
            if (launchMode == "standard") {
                isCreateActivity = true;
            } else if (launchMode == "singleTop") {
                if (targetTask->getTopActivity()->mActivityName == activityInfo.name) {
                    isCreateActivity = false;
                    record = targetTask->getTopActivity();
                }
            } else if (launchMode == "singleTask") {
                record = targetTask->findActivity(activityInfo.name);
                if (record) {
                    isCreateActivity = false;
                    /** The Activities above the target will be out of the stack */
                    targetTask->popToActivity(record);
                }
            } else if (launchMode == "singleInstance") {
                record = targetTask->getTopActivity();
                if (record) {
                    isCreateActivity = false;
                }
            }
        } else {
            if (intent.mFlag & Intent::FLAG_ACTIVITY_CLEAR_TASK) {
                /** clear all Activities, and new a target Activity. */
                targetTask->popAll();
            } else if (intent.mFlag & Intent::FLAG_ACTIVITY_CLEAR_TOP) {
                record = targetTask->findActivity(activityInfo.name);
                if (record) {
                    isCreateActivity = false;
                    targetTask->popToActivity(record);
                }
            } else if (intent.mFlag & Intent::FLAG_ACTIVITY_SINGLE_TOP) {
                if (targetTask->getTopActivity()->mActivityName == activityInfo.name) {
                    isCreateActivity = false;
                }
            }
        }

        mTaskManager.switchTaskToActive(targetTask);
    } else {
        mTaskManager.pushActiveTask(targetTask);
    }

    if (isCreateActivity) {
        sp<IBinder> token(new android::BBinder());
        record = std::make_shared<ActivityRecord>(activityInfo.name, token, caller, requestCode,
                                                  activityInfo.launchMode, app, targetTask,
                                                  mWindowManager);
        mActivityMap.emplace(token, record);
        targetTask->pushActivity(record);
        const auto startActivity = [this, record]() {
            const auto resumeActivity = [this, record]() { record->resume(); };
            mPendTask.commitTask(std::make_shared<ActivityReportStatusTask>(ActivityRecord::STARTED,
                                                                            record->mToken,
                                                                            resumeActivity));
            record->start();
        };
        mPendTask.commitTask(std::make_shared<ActivityReportStatusTask>(ActivityRecord::CREATED,
                                                                        token, startActivity));
        record->create();
    } else {
        record->mIntent = intent;
        const auto resumeActivityTask = [this, record]() { record->resume(); };
        mPendTask.commitTask(std::make_shared<ActivityReportStatusTask>(ActivityRecord::STARTED,
                                                                        record->mToken,
                                                                        resumeActivityTask));
        record->start();
    }

    return record;
}

bool ActivityManagerInner::finishActivity(const sp<IBinder>& token, int32_t resultCode,
                                          const std::optional<Intent>& resultData) {
    AM_PROFILER_BEGIN();
    auto currentActivity = getActivityRecord(token);
    if (!currentActivity) {
        ALOGE("finishActivity: The token is invalid");
        AM_PROFILER_END();
        return false;
    }
    ALOGD("finishActivity called by %s/%s", currentActivity->getPackageName()->c_str(),
          currentActivity->mActivityName.c_str());
    if (currentActivity->mStatus == ActivityRecord::RESUMED) {
        currentActivity->pause();

        const auto task = [this, currentActivity, resultCode, resultData]() {
            auto currentStack = currentActivity->mInTask.lock();
            /** when last pasued, then set result to next */
            const auto callActivity = getActivityRecord(currentActivity->mCaller);
            if (currentActivity->mRequestCode != ActivityManager::NO_REQUEST &&
                resultData.has_value() && callActivity) {
                callActivity->onResult(currentActivity->mRequestCode, resultCode,
                                       resultData.value());
            }
            currentStack->popActivity();
            auto nextActivity = currentStack->getTopActivity();
            if (!nextActivity) {
                /** nextActivity is null, the ActivityStack should be
                 * destroy */
                mTaskManager.popFrontTask();
                const auto activeTask = mTaskManager.getActiveTask();
                nextActivity = activeTask->getTopActivity();
            }

            /*resume next Activity then stop last Activity*/
            const auto destroyActivity = [this, currentActivity]() {
                currentActivity->stop();
                currentActivity->destroy();
                // TODO when report destroy. delete it from mActivityMap
            };
            mPendTask.commitTask(std::make_shared<ActivityReportStatusTask>(ActivityRecord::RESUMED,
                                                                            nextActivity->mToken,
                                                                            destroyActivity));
            const auto resumeActivityTask = [this, nextActivity]() { nextActivity->resume(); };
            mPendTask.commitTask(std::make_shared<ActivityReportStatusTask>(ActivityRecord::STARTED,
                                                                            nextActivity->mToken,
                                                                            resumeActivityTask));
            nextActivity->start();
        };
        const auto finishActivityTask =
                std::make_shared<ActivityReportStatusTask>(ActivityRecord::PAUSED, token, task);

        mPendTask.commitTask(finishActivityTask);
    } else {
        ALOGE("the Activity that being finished is inactive !!!");
        currentActivity->stop();
        currentActivity->destroy();
    }
    AM_PROFILER_END();
    return true;
}

void ActivityManagerInner::reportActivityStatus(const sp<IBinder>& token, int32_t status) {
    AM_PROFILER_BEGIN();
    auto record = getActivityRecord(token);
    if (!record) {
        ALOGE("The reported token is invalid");
        AM_PROFILER_END();
        return;
    }
    ALOGD("reportActivityStatus %s/%s status:%s->%s", record->getPackageName()->c_str(),
          record->mActivityName.c_str(), ActivityRecord::status2Str(record->mStatus),
          ActivityRecord::status2Str(status));
    switch (status) {
        case ActivityRecord::CREATED:
        case ActivityRecord::STARTED:
        case ActivityRecord::RESUMED:
        case ActivityRecord::PAUSED:
        case ActivityRecord::STOPPED: {
            break;
        }
        case ActivityRecord::DESTROYED: {
            mActivityMap.erase(token);
            if (auto appRecord = record->mApp.lock()) {
                appRecord->checkActiveStatus();
            }
            ALOGI("delete activity record, token[%p]", token.get());
        }
        default:
            break;
    }

    record->mStatus = status;
    const ActivityReportStatusTask::Event event(status, token);
    mPendTask.eventTrigger(event);
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

    ALOGD("start service:%s/%s", packageName.c_str(), serviceName.c_str());

    auto service = mServices.findService(packageName, serviceName);
    if (service) {
        service->start(intent);
    } else {
        const auto appRecord = mAppInfo.findAppInfo(packageName);
        if (appRecord) {
            const sp<IBinder> token(new android::BBinder());
            service = std::make_shared<ServiceRecord>(serviceName, token, appRecord);
            mServices.addService(service);
            service->start(intent);
        } else {
            PackageInfo targetApp;
            mPm.getPackageInfo(packageName, &targetApp);
            const int pid = AppSpawn::appSpawn(targetApp.execfile.c_str(), {packageName});
            if (pid > 0) {
                const auto task = [this, serviceName, packageName,
                                   intent](const AppAttachTask::Event* e) {
                    auto app = std::make_shared<AppRecord>(e->mAppHandler, packageName, e->mPid,
                                                           e->mUid);
                    this->mAppInfo.addAppInfo(app);
                    const sp<IBinder> token(new android::BBinder());
                    auto serviceRecord = std::make_shared<ServiceRecord>(serviceName, token, app);
                    mServices.addService(serviceRecord);
                    serviceRecord->start(intent);
                };
                mPendTask.commitTask(std::make_shared<AppAttachTask>(pid, task));
            } else {
                ALOGE("appSpawn App:%s error", targetApp.execfile.c_str());
                AM_PROFILER_END();
                return android::BAD_VALUE;
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

    ALOGD("stop service:%s/%s", packageName.c_str(), serviceName.c_str());

    auto service = mServices.findService(packageName, serviceName);
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

    ALOGD("bind service:%s/%s connection[%p]", packageName.c_str(), serviceName.c_str(),
          conn.get());

    auto service = mServices.findService(packageName, serviceName);
    if (!service) {
        const auto appRecord = mAppInfo.findAppInfo(packageName);
        if (appRecord) {
            const sp<IBinder> token(new android::BBinder());
            service = std::make_shared<ServiceRecord>(serviceName, token, appRecord);
            mServices.addService(service);
            service->bind(caller, conn, intent);
        } else {
            PackageInfo targetApp;
            mPm.getPackageInfo(packageName, &targetApp);
            const int pid = AppSpawn::appSpawn(targetApp.execfile.c_str(), {packageName});
            if (pid > 0) {
                const auto task = [this, serviceName, packageName, caller, conn,
                                   intent](const AppAttachTask::Event* e) {
                    auto app = std::make_shared<AppRecord>(e->mAppHandler, packageName, e->mPid,
                                                           e->mUid);
                    this->mAppInfo.addAppInfo(app);
                    const sp<IBinder> token(new android::BBinder());
                    auto serviceRecord = std::make_shared<ServiceRecord>(serviceName, token, app);
                    mServices.addService(serviceRecord);
                    serviceRecord->bind(caller, conn, intent);
                };
                mPendTask.commitTask(std::make_shared<AppAttachTask>(pid, task));
            } else {
                ALOGE("appSpawn App:%s error", targetApp.execfile.c_str());
                AM_PROFILER_END();
                return android::BAD_VALUE;
            }
        }
        const auto bindtask = [this, service, caller, intent, conn]() {
            service->bind(caller, conn, intent);
        };
        mPendTask.commitTask(std::make_shared<ServiceReportStatusTask>(ServiceRecord::STARTED,
                                                                       service->mToken, bindtask));
    } else {
        service->bind(caller, conn, intent);
    }
    AM_PROFILER_END();
    return 0;
}

void ActivityManagerInner::unbindService(const sp<IServiceConnection>& conn) {
    AM_PROFILER_BEGIN();
    ALOGD("unbindService connection[%p]", conn.get());
    mServices.unbindConnection(conn);
    AM_PROFILER_END();
    return;
}

void ActivityManagerInner::publishService(const sp<IBinder>& token,
                                          const sp<IBinder>& serviceBinder) {
    AM_PROFILER_BEGIN();
    auto service = mServices.getService(token);
    if (service) {
        service->mServiceBinder = serviceBinder;
    } else {
        ALOGE("publishService error. the Service token[%p] isn't exist", token.get());
    }
    AM_PROFILER_END();
}

int ActivityManagerInner::stopServiceToken(const sp<IBinder>& token) {
    AM_PROFILER_BEGIN();
    auto service = mServices.getService(token);
    if (!service) {
        ALOGW("unbelievable! Can't get record when service stop self:%s/%s",
              service->getPackageName()->c_str(), service->mServiceName.c_str());
        AM_PROFILER_END();
        return android::DEAD_OBJECT;
    }
    ALOGD("stopServiceToken. %s/%s", service->getPackageName()->c_str(),
          service->mServiceName.c_str());
    stopServiceReal(service);
    AM_PROFILER_END();
    return 0;
}

void ActivityManagerInner::stopServiceReal(ServiceHandler& service) {
    ALOGI("stopService %s/%s", service->getPackageName()->c_str(), service->mServiceName.c_str());
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
    ALOGD("reportServiceStatus %s/%s status:%s->%s", service->getPackageName()->c_str(),
          service->mServiceName.c_str(), ServiceRecord::status2Str(service->mStatus),
          ServiceRecord::status2Str(status));
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
                appRecord->checkActiveStatus();
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
    ALOGD("sendBroadcast:%s", intent.mAction.c_str());
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
    ALOGD("registerReceiver:%s", action.c_str());
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
    ALOGD("unregisterReceiver");
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
    for (auto& it : appRecord->mExistActivity) {
        if (auto activityRecord = it.lock()) {
            activityRecord->abnormalExit();
            mTaskManager.procAbnormalActivity(activityRecord);
            mActivityMap.erase(activityRecord->mToken);
        }
    }

    for (auto& it : appRecord->mExistService) {
        if (auto serviceRecord = it.lock()) {
            serviceRecord->abnormalExit();
            mServices.deleteService(serviceRecord->mToken);
        }
    }

    auto topActivity = mTaskManager.getActiveTask()->getTopActivity();
    if (topActivity->mStatus > ActivityRecord::RESUMED) {
        if (topActivity->mStatus > ActivityRecord::STOPPING) {
            topActivity->start();
            topActivity->resume();
        } else {
            topActivity->resume();
        }
    }
}

void ActivityManagerInner::dump(int fd, const android::Vector<android::String16>& args) {
    std::ostringstream os;
    os << mTaskManager;
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
        mTaskManager.initHomeTask(std::make_shared<ActivityStack>(entryActivity.taskAffinity));
        const int pid = AppSpawn::appSpawn(homeApp.execfile.c_str(), {packageName});
        if (pid > 0) {
            auto task = std::make_shared<
                    AppAttachTask>(pid,
                                   [this, packageName,
                                    entryActivity](const AppAttachTask::Event* e) {
                                       auto appRecord =
                                               std::make_shared<AppRecord>(e->mAppHandler,
                                                                           packageName, e->mPid,
                                                                           e->mUid);
                                       this->mAppInfo.addAppInfo(appRecord);
                                       Intent intent;
                                       intent.setFlag(Intent::FLAG_ACTIVITY_NEW_TASK);
                                       this->startActivityReal(appRecord, entryActivity, intent,
                                                               sp<IBinder>(nullptr),
                                                               ActivityManager::NO_REQUEST);
                                   });
            mPendTask.commitTask(task);
        } else {
            ALOGE("appSpawn Home App:%s error", homeApp.execfile.c_str());
            return android::BAD_VALUE;
        }
    } else {
        ALOGE("systemReady error: can't launch Home Activity");
        return android::NAME_NOT_FOUND;
    }

    return android::OK;
}

ActivityHandler ActivityManagerInner::getActivityRecord(const sp<IBinder>& token) {
    auto iter = mActivityMap.find(token);
    if (iter != mActivityMap.end()) {
        return iter->second;
    } else {
        return nullptr;
    }
}

void getPackageAndComponentName(const string& target, string& packageName, string& componentName) {
    const auto pos = target.find_first_of('/');
    packageName = std::move(target.substr(0, pos));
    componentName =
            std::move(pos == std::string::npos ? "" : target.substr(pos + 1, std::string::npos));
}

ActivityManagerService::ActivityManagerService() {
    mInner = new ActivityManagerInner();
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

Status ActivityManagerService::finishActivity(const sp<IBinder>& token, int32_t resultCode,
                                              const std::optional<Intent>& resultData, bool* ret) {
    *ret = mInner->finishActivity(token, resultCode, resultData);
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

Status ActivityManagerService::stopServiceToken(const sp<IBinder>& token, int32_t* ret) {
    *ret = mInner->stopServiceToken(token);
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