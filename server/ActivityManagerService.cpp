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

#include "am/ActivityManagerService.h"

#include <binder/IPCThreadState.h>
#include <kvdb.h>
#include <pm/PackageManager.h>

#include <filesystem>
#include <fstream>
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
#include "TaskManager.h"
#include "app/ActivityManager.h"
#include "app/Logger.h"
#include "app/UvLoop.h"

namespace os {
namespace am {

using namespace std;
using namespace os::app;
using namespace os::pm;

using android::IBinder;
using android::sp;

#ifdef CONFIG_AMS_RUNMODE_FILE
#define AMS_RUNMODE_FILE CONFIG_AMS_RUNMODE_FILE
#else
#define AMS_RUNMODE_FILE "/data/ams.runmode"
#endif

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
    enum RunMode {
        NORMAL_MODE = 0,
        SILENCE_MODE,
        DEBUG_MODE,
    };
    ActivityManagerInner(uv_loop_t* looper);

    int attachApplication(const sp<IApplicationThread>& app);
    int startActivity(const sp<IBinder>& token, const Intent& intent, int32_t requestCode);
    int stopActivity(const Intent& intent, int32_t resultCode);
    int stopApplication(const sp<IBinder>& token);
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

    int32_t postIntent(const Intent& intent);
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
    int startActivityReal(ITaskManager* taskmanager, const string& activityName,
                          PackageInfo& packageInfo, const Intent& intent, const sp<IBinder>& caller,
                          const int32_t requestCode);
    int startServiceReal(const string& serviceName, PackageInfo& packageInfo, const Intent& intent,
                         const bool isBind, const sp<IBinder>& caller,
                         const sp<IServiceConnection>& conn);
    int intentToSingleTarget(const Intent& intent, PackageInfo& packageInfo, string& componentName,
                             const IntentAction::ComponentType type);
    int intentToMultiTarget(const Intent& intent, vector<PackageInfo>& packageInfoList,
                            vector<string>& componentNameList,
                            const IntentAction::ComponentType type);
    int broadcastIntent(const Intent& intent, const IntentAction::ComponentType type);
    void stopServiceReal(ServiceHandler& service);
    bool startBootGuide();
    int startHomeActivity();
    int submitAppStartupTask(const string& packageName, const string& prcocessName,
                             const string& execfile, AppAttachTask::TaskFunc&& task,
                             bool isSupportMultiTask);
    int findSystemTarget(const string& targetAlias, std::shared_ptr<AppRecord>& app,
                         sp<IBinder>& token);
    ActivityHandler getActivity(const sp<IBinder>& token);
    inline ITaskManager* getTaskManager(bool isSystemUI);
    inline ActivityHandler getTopActivity();

private:
    int mRunMode;
    std::shared_ptr<UvLoop> mLooper;
    std::map<sp<IBinder>, ActivityHandler> mActivityMap;
    TaskBoard mPendTask;
    ServiceList mServices;
    AppInfoList mAppInfo;
    TaskManagerFactory mTaskManager;
    IntentAction mActionFilter;
    PackageManager mPm;
    sp<::os::wm::IWindowManager> mWindowManager;
    map<string, list<sp<IBroadcastReceiver>>> mReceivers; /** Broadcast */
    LowMemoryManager mLmk;
    ProcessPriorityPolicy mPriorityPolicy;
    AppSpawn mAppSpawn;
};

ActivityManagerInner::ActivityManagerInner(uv_loop_t* looper) : mPriorityPolicy(&mLmk) {
    mRunMode = NORMAL_MODE;
    if (std::filesystem::exists(AMS_RUNMODE_FILE)) {
        std::ifstream file;
        file.open(AMS_RUNMODE_FILE);
        if (file.is_open()) {
            string line;
            std::getline(file, line);
            sscanf(line.c_str(), "%d", &mRunMode);
        }
    }
    mPendTask.setDebugMode(mRunMode == DEBUG_MODE);
    mTaskManager.init(mPendTask);
    mLooper = std::make_shared<UvLoop>(looper);
    mPendTask.startWork(mLooper);
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
        PackageInfo packageinfo;
        mPm.getPackageInfo(packageName, &packageinfo);
        appRecord = std::make_shared<AppRecord>(app, packageName, packageinfo.isSystemUI, callerPid,
                                                callerUid, &mAppInfo, &mPriorityPolicy);
        mAppInfo.deleteAppWaitingAttach(callerPid);
        mAppInfo.addAppInfo(appRecord);
        const AppAttachTask::Event event(callerPid, appRecord);
        mPendTask.eventTrigger(event);

        // broadcast app start
        Intent intent;
        intent.setAction(Intent::BROADCAST_APP_START);
        intent.setData(packageName);
        sendBroadcast(intent);
    } else {
        ALOGE("the application:%d attaching is illegally", callerPid);
    }

    AM_PROFILER_END();
    return android::OK;
}

int ActivityManagerInner::startActivity(const sp<IBinder>& caller, const Intent& intent,
                                        int32_t requestCode) {
    AM_PROFILER_BEGIN();
    ALOGI("start activity, target:%s action:%s %s flag:%" PRId32 "", intent.mTarget.c_str(),
          intent.mAction.c_str(), intent.mData.c_str(), intent.mFlag);
    int ret = android::OK;
    PackageInfo packageInfo;
    string activityName;
    if (intentToSingleTarget(intent, packageInfo, activityName, IntentAction::COMP_TYPE_ACTIVITY) !=
        0) {
        AM_PROFILER_END();
        return android::BAD_VALUE;
    }

    auto taskmanager = getTaskManager(packageInfo.isSystemUI);
    ActivityStackHandler apptask;
    /** check activity name */
    if (activityName.empty()) {
        apptask = taskmanager->findTask(packageInfo.packageName);
        if (!apptask) {
            activityName = packageInfo.entry;
        }
    }
    if (apptask) {
        taskmanager->switchTaskToActive(apptask, intent);
    } else {
        ret = startActivityReal(taskmanager, activityName, packageInfo, intent, caller,
                                requestCode);
    }

    if (!packageInfo.isSystemUI) {
        // 当有应用切换，给SystemUI任务栈发送消息
        mTaskManager.getManager(SystemUIMode)->onEvent(TaskManagerEvent::StartActivityEvent);
    }

    AM_PROFILER_END();
    return ret;
}

int ActivityManagerInner::startActivityReal(ITaskManager* taskmanager, const string& activityName,
                                            PackageInfo& packageInfo, const Intent& intent,
                                            const sp<IBinder>& caller, const int32_t requestCode) {
    AM_PROFILER_BEGIN();
    /** We need to check that the Intent.flag makes sense and perhaps modify it */
    string taskAffinity;
    int startFlag = intent.mFlag;
    ActivityRecord::LaunchMode launchMode = ActivityRecord::LaunchMode::SINGLE_TASK;
    std::vector<ActivityInfo>::iterator it = packageInfo.activitiesInfo.begin();
    for (; it != packageInfo.activitiesInfo.end(); ++it) {
        if (it->name == activityName) {
            launchMode = ActivityRecord::launchModeToInt(it->launchMode);
            taskAffinity = it->taskAffinity;
            break;
        }
    }
    if (it == packageInfo.activitiesInfo.end()) {
        ALOGE("Activity:%s/%s is not registered", packageInfo.packageName.c_str(),
              activityName.c_str());
        AM_PROFILER_END();
        return android::BAD_VALUE;
    }

    /** Entry Activity taskAffinity can only be packagename */
    if (activityName == packageInfo.entry) {
        taskAffinity = packageInfo.packageName;
        /** The entry activity must be taskTag for the taskStack, so we need it in another task
         */
        startFlag |= Intent::FLAG_ACTIVITY_NEW_TASK;
        if (launchMode != ActivityRecord::SINGLE_TASK) {
            /** Entry Activity only support singleTask or singleInstance */
            launchMode = ActivityRecord::SINGLE_INSTANCE;
        }
    }

    auto callActivity = getActivity(caller);
    if (!callActivity || callActivity->getLaunchMode() == ActivityRecord::SINGLE_INSTANCE ||
        launchMode == ActivityRecord::SINGLE_INSTANCE) {
        /** if the caller's(maybe Service) Stack does't exist, we must new a task */
        startFlag |= Intent::FLAG_ACTIVITY_NEW_TASK;
    }

    ActivityStackHandler targetTask;
    bool isNewTask = false;
    if (startFlag & Intent::FLAG_ACTIVITY_NEW_TASK) {
        targetTask = taskmanager->findTask(taskAffinity);
        if (!targetTask) {
            targetTask = std::make_shared<ActivityStack>(taskAffinity);
            isNewTask = true;
        }
    } else {
        targetTask = taskmanager->getActiveTask();
    }

    ActivityHandler targetActivity;
    string activityUniqueName = packageInfo.packageName + "/" + activityName;
    if (!isNewTask) {
        switch (launchMode) {
            case ActivityRecord::SINGLE_TOP:
                startFlag |= Intent::FLAG_ACTIVITY_SINGLE_TOP;
                if (targetTask->getTopActivity()->getName() == activityUniqueName) {
                    targetActivity = targetTask->getTopActivity();
                }
                break;
            case ActivityRecord::SINGLE_TASK:
            case ActivityRecord::SINGLE_INSTANCE:
                startFlag |= Intent::FLAG_ACTIVITY_CLEAR_TOP;
                targetActivity = targetTask->findActivity(activityUniqueName);
                break;
            default:
                break;
        }
    }

    if (!targetActivity) {
        auto newActivity =
                std::make_shared<ActivityRecord>(activityUniqueName, caller, requestCode,
                                                 launchMode, targetTask, intent, mWindowManager,
                                                 taskmanager, &mPendTask);
        const auto appInfo = mAppInfo.findAppInfoWithAlive(packageInfo.packageName);
        if (appInfo) {
            newActivity->setAppThread(appInfo);
            taskmanager->pushNewActivity(targetTask, newActivity, startFlag);
        } else {
            // Check the system environment is adequate for starting the application
            if (!mLmk.isOkToLaunch()) {
                ALOGE("check launch envirnoment, can't start new application");
                AM_PROFILER_END();
                return android::INVALID_OPERATION;
            }

            const ProcessPriority priority = (ProcessPriority)packageInfo.priority;
            const auto task = [this, taskmanager, targetTask, newActivity, startFlag,
                               priority](const AppAttachTask::Event* e) {
                mPriorityPolicy.add(e->mPid, true, priority);
                newActivity->setAppThread(e->mAppRecord);
                taskmanager->pushNewActivity(targetTask, newActivity, startFlag);
            };
            if (submitAppStartupTask(packageInfo.packageName, packageInfo.packageName,
                                     packageInfo.execfile, std::move(task), false) != 0) {
                ALOGW("submitAppStartupTask failure");
                AM_PROFILER_END();
                return android::INVALID_OPERATION;
            }
        }
        mActivityMap[newActivity->getToken()] = newActivity;

    } else {
        /** if there is no need to create an Activity, caller/requestCode is invalid */
        taskmanager->turnToActivity(targetTask, targetActivity, intent, startFlag);
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
            auto taskmanager = getTaskManager(appinfo->mIsSystemUI);
            if (activityName.empty()) {
                auto activetask = taskmanager->getActiveTask();
                if (activetask && activetask->getTaskTag() == appinfo->mPackageName) {
                    // move to back if the app is active task at first
                    taskmanager->moveTaskToBackground(activetask);
                }
                if (intent.mFlag != Intent::FLAG_APP_MOVE_BACK) {
                    appinfo->stopApplication();
                }
            } else {
                activity = appinfo->checkActivity(intent.mTarget);
                if (activity) {
                    const auto callActivity = getActivity(activity->getCaller());
                    if (activity->getRequestCode() != ActivityManager::NO_REQUEST && callActivity) {
                        callActivity->onResult(activity->getRequestCode(), resultCode, intent);
                    }
                    taskmanager->finishActivity(activity);
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

int ActivityManagerInner::stopApplication(const sp<IBinder>& token) {
    AM_PROFILER_BEGIN();
    int ret = android::OK;
    std::shared_ptr<AppRecord> app;

    if (auto activity = getActivity(token)) {
        app = activity->getAppRecord();
        auto taskmanager = getTaskManager(app->mIsSystemUI);
        auto task = activity->getTask();
        if (task && task == taskmanager->getActiveTask()) {
            // if the Activity is top task, we need move the task to background.
            taskmanager->moveTaskToBackground(task);
        }
    } else if (auto service = mServices.getService(token)) {
        app = service->mApp.lock();
    }

    if (app) {
        ALOGW("stopApplication target:%s by token[%p]", app->mPackageName.c_str(), token.get());
        app->stopApplication();
    } else {
        ALOGE("stopApplication by illegal components[%p]", token.get());
        ret = android::BAD_VALUE;
    }

    AM_PROFILER_END();
    return ret;
}

bool ActivityManagerInner::finishActivity(const sp<IBinder>& token, int32_t resultCode,
                                          const std::optional<Intent>& resultData) {
    AM_PROFILER_BEGIN();
    auto activity = getActivity(token);
    if (!activity) {
        ALOGE("finishActivity: The token is invalid");
        AM_PROFILER_END();
        return false;
    }
    ALOGI("finishActivity called by %s", activity->getName().c_str());

    /** when last pasued, then set result to next */
    const auto callActivity = getActivity(activity->getCaller());
    if (activity->getRequestCode() != ActivityManager::NO_REQUEST && resultData.has_value() &&
        callActivity) {
        callActivity->onResult(activity->getRequestCode(), resultCode, resultData.value());
    }

    auto taskmanager = getTaskManager(activity->getAppRecord()->mIsSystemUI);
    taskmanager->finishActivity(activity);

    AM_PROFILER_END();
    return true;
}

bool ActivityManagerInner::moveActivityTaskToBackground(const sp<IBinder>& token, bool nonRoot) {
    AM_PROFILER_BEGIN();
    bool ret = false;
    auto activity = getActivity(token);
    if (activity) {
        ALOGI("moveActivityTaskToBackground, activity:%s nonRoot:%s", activity->getName().c_str(),
              nonRoot ? "true" : "false");
        auto activityTask = activity->getTask();
        if (activityTask && (nonRoot || activityTask->getRootActivity() == activity)) {
            auto taskmanager = getTaskManager(activity->getAppRecord()->mIsSystemUI);
            ret = taskmanager->moveTaskToBackground(activityTask);
        }
    } else {
        ALOGE("moveActivityTaskToBackground: The token is invalid");
    }

    AM_PROFILER_END();
    return ret;
}

void ActivityManagerInner::reportActivityStatus(const sp<IBinder>& token, int32_t status) {
    AM_PROFILER_BEGIN();
    auto activity = getActivity(token);
    if (!activity) {
        ALOGW("reportActivityStatus error: activity is null");
        AM_PROFILER_END();
        return;
    }
    ALOGI("reportActivityStatus called by %s [%s]", getActivity(token)->getName().c_str(),
          ActivityRecord::statusToStr(status));

    const ActivityLifeCycleTask::Event event((ActivityRecord::Status)status, token);
    mPendTask.eventTrigger(event);

    const auto broadcastTopActivity = [this](const string& name) {
        Intent intent;
        intent.setAction(Intent::BROADCAST_TOP_ACTIVITY);
        intent.setData(name);
        sendBroadcast(intent);
    };

    // Only "destroy" need special process.
    switch (status) {
        case ActivityRecord::CREATED:
        case ActivityRecord::STARTED:
            break;
        case ActivityRecord::RESUMED: {
            // broadcast the Top Activity
            const auto taskmanager = mTaskManager.getManager(TaskManagerType::StandardMode);
            if (activity == taskmanager->getActiveTask()->getTopActivity()) {
                broadcastTopActivity(activity->getName());
            }
            const auto appRecord = activity->getAppRecord();
            if (appRecord && !appRecord->mIsSystemUI) {
                const ActivityWaitResume::Event event2(token);
                mPendTask.eventTrigger(event2);

                // only for finishActivity case
                const ActivityDelayDestroy::Event event3(token);
                mPendTask.eventTrigger(event3);
            }
            break;
        }
        case ActivityRecord::PAUSED:
            break;
        case ActivityRecord::STOPPED: {
            const auto appRecord = activity->getAppRecord();
            if (appRecord && appRecord->mIsSystemUI) {
                // systemui不会改变其他应用生命周期，需要主动查询当前顶层resume应用
                auto nextTopActivity = getTopActivity();
                if (nextTopActivity) {
                    broadcastTopActivity(nextTopActivity->getName());
                }
            }
            break;
        }
        case ActivityRecord::DESTROYED: {
            activity->setStatus(ActivityRecord::DESTROYED);
            if (const auto appRecord = activity->getAppRecord()) {
                auto taskmanager = getTaskManager(appRecord->mIsSystemUI);
                taskmanager->deleteActivity(activity);
                appRecord->deleteActivity(activity);
                if (!appRecord->checkActiveStatus()) {
                    appRecord->stopApplication();
                }
            }
            mActivityMap.erase(activity->getToken());
            break;
        }
    }

    AM_PROFILER_END();
    return;
}

int ActivityManagerInner::startService(const Intent& intent) {
    AM_PROFILER_BEGIN();
    ALOGI("start service, target:%s action:%s data:%s flag:%" PRId32 "", intent.mTarget.c_str(),
          intent.mAction.c_str(), intent.mData.c_str(), intent.mFlag);

    PackageInfo packageInfo;
    string serviceName;
    int ret = android::BAD_VALUE;
    if (intentToSingleTarget(intent, packageInfo, serviceName, IntentAction::COMP_TYPE_SERVICE) ==
                0 &&
        startServiceReal(serviceName, packageInfo, intent, false, nullptr, nullptr) == 0) {
        ret = android::OK;
    }

    AM_PROFILER_END();
    return ret;
}

int ActivityManagerInner::startServiceReal(const string& serviceName, PackageInfo& packageInfo,
                                           const Intent& intent, bool isBind,
                                           const sp<IBinder>& caller,
                                           const sp<IServiceConnection>& conn) {
    ProcessPriority priority = ProcessPriority::PERSISTENT;
    std::vector<ServiceInfo>::iterator it;
    for (it = packageInfo.servicesInfo.begin(); it != packageInfo.servicesInfo.end(); ++it) {
        if (it->name == serviceName) {
            priority = (ProcessPriority)it->priority;
            break;
        }
    }
    if (it == packageInfo.servicesInfo.end()) {
        ALOGE("service:%s/%s is not registered", packageInfo.packageName.c_str(),
              serviceName.c_str());
        return -1;
    }

    // service maybe runs in a stand-alone process
    string servicePackageName;
    string serviceExecBin;
    if (packageInfo.appType == APP_TYPE_QUICK) {
        // bad demand for quick service...
        servicePackageName = VSERVICE_EXEC_NAME + ':' + packageInfo.packageName;
        serviceExecBin = VSERVICE_EXEC_NAME;
    } else {
        servicePackageName = packageInfo.packageName;
        serviceExecBin = packageInfo.execfile;
    }

    ServiceHandler service = mServices.findService(servicePackageName, serviceName);
    if (service) {
        if (!isBind) {
            service->start(intent);
        } else {
            service->bind(caller, conn, intent);
        }
    } else {
        std::shared_ptr<AppRecord> appRecord;
        appRecord = mAppInfo.findAppInfoWithAlive(servicePackageName);
        if (appRecord) {
            const sp<IBinder> token(new android::BBinder());
            service = std::make_shared<ServiceRecord>(serviceName, token, priority, appRecord);
            if (auto prioritynode = mPriorityPolicy.get(appRecord->mPid)) {
                if (prioritynode->priorityLevel < priority) {
                    prioritynode->priorityLevel = priority;
                }
            }
            mServices.addService(service);
            if (!isBind) {
                service->start(intent);
            } else {
                service->bind(caller, conn, intent);
            }
        } else {
            const auto task = [this, serviceName, intent, priority, caller, conn,
                               isBind](const AppAttachTask::Event* e) {
                const sp<IBinder> token(new android::BBinder());
                auto serviceHandler = std::make_shared<ServiceRecord>(serviceName, token, priority,
                                                                      e->mAppRecord);
                mPriorityPolicy.add(e->mPid, false, priority);
                mServices.addService(serviceHandler);
                if (!isBind) {
                    serviceHandler->start(intent);
                } else {
                    serviceHandler->bind(caller, conn, intent);
                }
            };
            if (submitAppStartupTask(packageInfo.packageName, servicePackageName, serviceExecBin,
                                     std::move(task), true) != 0) {
                ALOGW("submitAppStartupTask failure");
                return -2;
            }
        }
    }

    return 0;
}

int ActivityManagerInner::stopService(const Intent& intent) {
    AM_PROFILER_BEGIN();
    PackageInfo packageInfo;
    string serviceName;
    if (intentToSingleTarget(intent, packageInfo, serviceName, IntentAction::COMP_TYPE_SERVICE) !=
        0) {
        AM_PROFILER_END();
        return android::DEAD_OBJECT;
    }

    // service maybe runs in a stand-alone process
    string servicePackageName;
    if (packageInfo.appType == APP_TYPE_QUICK) {
        // bad demand for quick service...
        servicePackageName = VSERVICE_EXEC_NAME + ':' + packageInfo.packageName;
    } else {
        servicePackageName = packageInfo.packageName;
    }

    ServiceHandler service = mServices.findService(servicePackageName, serviceName);
    if (!service) {
        ALOGW("the Service:%s is not running", serviceName.c_str());
        AM_PROFILER_END();
        return android::DEAD_OBJECT;
    }

    ALOGI("stopService %s/%s", service->getPackageName()->c_str(), service->mServiceName.c_str());
    stopServiceReal(service);

    AM_PROFILER_END();
    return android::OK;
}

int ActivityManagerInner::bindService(const sp<IBinder>& caller, const Intent& intent,
                                      const sp<IServiceConnection>& conn) {
    AM_PROFILER_BEGIN();
    ALOGI("bindService, target:%s action:%s data:%s flag:%" PRId32 "", intent.mTarget.c_str(),
          intent.mAction.c_str(), intent.mData.c_str(), intent.mFlag);

    PackageInfo packageInfo;
    string serviceName;
    int ret = android::OK;
    if (intentToSingleTarget(intent, packageInfo, serviceName, IntentAction::COMP_TYPE_SERVICE) ==
        0) {
        if (startServiceReal(serviceName, packageInfo, intent, true, caller, conn) != 0) {
            ret = android::INVALID_OPERATION;
        }
    } else {
        ret = android::BAD_VALUE;
    }

    AM_PROFILER_END();
    return ret;
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
        ALOGW("unbelievable! Can't get record when service stop self");
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
            ALOGE("unbeliveable!!! service status:%" PRId32 " is illegal", status);
            AM_PROFILER_END();
            return;
        }
    }
    service->mStatus = status;
    const ServiceReportStatusTask::Event event(status, token);
    mPendTask.eventTrigger(event);
    AM_PROFILER_END();
}

int32_t ActivityManagerInner::postIntent(const Intent& intent) {
    AM_PROFILER_BEGIN();
    ALOGI("postIntent:%s", intent.mTarget.c_str());
    std::shared_ptr<AppRecord> app = nullptr;
    sp<IBinder> token = nullptr;

    if (intent.mTarget.compare(0, Intent::TARGET_PREFLEX.size(), Intent::TARGET_PREFLEX) == 0) {
        findSystemTarget(intent.mTarget, app, token);
    } else {
        string packageName;
        string componentName;
        getPackageAndComponentName(intent.mTarget, packageName, componentName);
        app = mAppInfo.findAppInfoWithAlive(packageName);
        if (app) {
            if (componentName.empty()) {
                // Not Activity or Service, the Intent will be posted to Application.
                token = android::IInterface::asBinder(app->mAppThread);
            } else {
                if (const auto activity = app->checkActivity(intent.mTarget)) {
                    token = activity->getToken();
                } else {
                    if (const auto service = app->checkService(intent.mTarget)) {
                        token = service->mToken;
                    }
                }
            }
        }
    }

    if (!app || !token) {
        ALOGW("postIntent target:%s is nonexist!!", intent.mTarget.c_str());
        AM_PROFILER_END();
        return -1;
    }
    app->scheduleReceiveIntent(token, intent);
    AM_PROFILER_END();
    return 0;
}

int32_t ActivityManagerInner::sendBroadcast(const Intent& intent) {
    AM_PROFILER_BEGIN();
    ALOGD("sendBroadcast:%s", intent.mAction.c_str());
    auto receivers = mReceivers.find(intent.mAction);
    if (receivers != mReceivers.end()) {
        for (auto& receiver : receivers->second) {
            receiver->receiveBroadcast(intent);
        }
    }
    AM_PROFILER_END();
    return 0;
}

int32_t ActivityManagerInner::registerReceiver(const std::string& action,
                                               const sp<IBroadcastReceiver>& receiver) {
    AM_PROFILER_BEGIN();
    ALOGI("registerReceiver:%s", action.c_str());
    auto receivers = mReceivers.find(action);
    if (receivers != mReceivers.end()) {
        receivers->second.emplace_back(receiver);
        ALOGI("register success, cnt:%d", receivers->second.size());
    } else {
        std::list<sp<IBroadcastReceiver>> receiverList;
        receiverList.emplace_back(receiver);
        mReceivers.emplace(action, std::move(receiverList));
        ALOGD("add new receiver success");
    }
    AM_PROFILER_END();
    return 0;
}

void ActivityManagerInner::unregisterReceiver(const sp<IBroadcastReceiver>& receiver) {
    AM_PROFILER_BEGIN();
    ALOGI("unregisterReceiver");
    for (auto iter = mReceivers.begin(); iter != mReceivers.end();) {
        for (auto it = iter->second.begin(); it != iter->second.end(); ++it) {
            if (android::IInterface::asBinder(*it) == android::IInterface::asBinder(receiver)) {
                iter->second.erase(it);
                break;
            }
        }
        if (iter->second.empty()) {
            iter = mReceivers.erase(iter);
        } else {
            ++iter;
        }
    }

    AM_PROFILER_END();
}

int ActivityManagerInner::intentToSingleTarget(const Intent& intent, PackageInfo& packageInfo,
                                               string& componentName,
                                               IntentAction::ComponentType type) {
    AM_PROFILER_BEGIN();
    string packageName;
    if (intent.mTarget.empty()) {
        string target;
        mActionFilter.getSingleTargetByAction(intent.mAction, target, type);
        getPackageAndComponentName(target, packageName, componentName);
    } else {
        getPackageAndComponentName(intent.mTarget, packageName, componentName);
    }

    if (packageName.empty() || mPm.getPackageInfo(packageName, &packageInfo) != 0) {
        ALOGE("can't find target by intent[%s,%s]", intent.mTarget.c_str(), intent.mAction.c_str());
        AM_PROFILER_END();
        return -1;
    }
    AM_PROFILER_END();
    return 0;
}

int ActivityManagerInner::intentToMultiTarget(const Intent& intent,
                                              vector<PackageInfo>& packageInfoList,
                                              vector<string>& componentNameList,
                                              const IntentAction::ComponentType type) {
    AM_PROFILER_BEGIN();
    vector<string> targetlist;
    if (intent.mTarget.empty()) {
        string target;
        mActionFilter.getMultiTargetByAction(intent.mAction, targetlist, type);
    } else {
        targetlist.push_back(intent.mTarget);
    }

    string packageName;
    string componentName;
    PackageInfo packageInfo;
    componentNameList.clear();
    packageInfoList.clear();
    for (auto& target : targetlist) {
        getPackageAndComponentName(target, packageName, componentName);
        if (packageName.empty() || mPm.getPackageInfo(packageName, &packageInfo) != 0) {
            ALOGE("can't find target by intent[%s,%s]", intent.mTarget.c_str(),
                  intent.mAction.c_str());
            AM_PROFILER_END();
            return -1;
        }
        packageInfoList.push_back(packageInfo);
        componentNameList.push_back(componentName);
    }
    AM_PROFILER_END();
    return 0;
}

int ActivityManagerInner::broadcastIntent(const Intent& intent,
                                          const IntentAction::ComponentType type) {
    AM_PROFILER_BEGIN();
    vector<PackageInfo> packageList;
    vector<string> componentList;
    if (intentToMultiTarget(intent, packageList, componentList, type) != 0) {
        AM_PROFILER_END();
        return -1;
    }
    const int size = componentList.size();
    for (int i = 0; i < size; i++) {
        auto taskmanager = getTaskManager(packageList[i].isSystemUI);
        if (type == IntentAction::COMP_TYPE_ACTIVITY) {
            startActivityReal(taskmanager, componentList[i], packageList[i], intent, nullptr, -1);
        } else if (type == IntentAction::COMP_TYPE_SERVICE) {
            startServiceReal(componentList[i], packageList[i], intent, false, nullptr, nullptr);
        }
    }
    AM_PROFILER_END();
    return 0;
}

void ActivityManagerInner::systemReady() {
    AM_PROFILER_BEGIN();
    ALOGD("### systemReady ### ");
    mAppSpawn.signalInit(mLooper->get(), [this](int pid) {
        ALOGW("AppSpawn pid:%d had exit", pid);
        auto app = mAppInfo.findAppInfo(pid);
        if (app) {
            procAppTerminated(app);
            mAppInfo.deleteAppInfo(pid);
            mPriorityPolicy.remove(pid);
        } else {
            string packagename;
            if (mAppInfo.getAttachingAppName(pid, packagename)) {
                ALOGE("App:%s abnormal exit without attachApplication", packagename.c_str());
                mAppInfo.deleteAppWaitingAttach(pid);
            }
        }

        if (!mTaskManager.getManager(StandardMode)->getActiveTask() && mRunMode == NORMAL_MODE) {
            startHomeActivity();
        }
    });

    if (mRunMode > NORMAL_MODE) {
        ALOGW("AMS run mode[%d], apps don't start automatically", mRunMode);
        AM_PROFILER_END();
        return;
    }

    // After the system ready, broadcast ACTION_BOOT_READY to start Activity and Service
    Intent intent;
    intent.setAction(Intent::ACTION_BOOT_READY);
    broadcastIntent(intent, IntentAction::COMP_TYPE_SERVICE);
    broadcastIntent(intent, IntentAction::COMP_TYPE_ACTIVITY);

    if (startBootGuide() == false) {
        startHomeActivity();
    }

    //  broadcast ACTION_BOOT_COMPLETED to start Activity and Service
    intent.setAction(Intent::ACTION_BOOT_COMPLETED);
    broadcastIntent(intent, IntentAction::COMP_TYPE_SERVICE);
    broadcastIntent(intent, IntentAction::COMP_TYPE_ACTIVITY);

    AM_PROFILER_END();
    return;
}

void ActivityManagerInner::procAppTerminated(const std::shared_ptr<AppRecord>& appRecord) {
    AM_PROFILER_BEGIN();
    /** All activity needs to be destroyed from the stack */
    appRecord->mStatus = APP_STOPPED;
    std::vector<std::weak_ptr<ActivityRecord>> needDeleteActivity;
    needDeleteActivity.swap(appRecord->mExistActivity);
    /** First mark all Destroy Activity. */
    for (auto& it : needDeleteActivity) {
        if (auto activityRecord = it.lock()) {
            activityRecord->abnormalExit();
        }
    }
    /** Remove from task stack */
    auto taskmanager = getTaskManager(appRecord->mIsSystemUI);
    for (auto& it : needDeleteActivity) {
        if (auto activityRecord = it.lock()) {
            taskmanager->deleteActivity(activityRecord);
            mActivityMap.erase(activityRecord->getToken());
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

    // broadcast app exit
    Intent intent;
    intent.setAction(Intent::BROADCAST_APP_EXIT);
    intent.setData(appRecord->mPackageName);
    sendBroadcast(intent);

    AM_PROFILER_END();
}

void ActivityManagerInner::dump(int fd, const android::Vector<android::String16>& args) {
    std::ostringstream os;
    os << mTaskManager << mServices << mPriorityPolicy;
    write(fd, os.str().c_str(), os.str().size());
}

bool ActivityManagerInner::startBootGuide() {
    AM_PROFILER_BEGIN();
    const char* usersetup = "persist.global.system.usersetup_complete";
    bool ret = false;
    int8_t defvalue = 0;
    int iscomplete = property_get_bool(usersetup, defvalue);
    if (!iscomplete) {
        /** start the bootguide app */
        Intent intent;
        intent.setAction(Intent::ACTION_BOOT_GUIDE);
        sp<IBinder> faketoken;
        if (startActivity(faketoken, intent, (int32_t)ActivityManager::NO_REQUEST) == android::OK) {
            ret = true;
        }
    }
    AM_PROFILER_END();
    return ret;
}

int ActivityManagerInner::startHomeActivity() {
    /** start the launch app */
    AM_PROFILER_BEGIN();
    int ret = 0;
    Intent intent;
    intent.setAction(Intent::ACTION_HOME);
    sp<IBinder> faketoken;
    if (startActivity(faketoken, intent, (int32_t)ActivityManager::NO_REQUEST) != android::OK) {
        ALOGE("Startup home app failure!!!");
        ret = -1;
    }
    AM_PROFILER_END();
    return ret;
}

int ActivityManagerInner::submitAppStartupTask(const string& packageName,
                                               const string& prcocessName, const string& execfile,
                                               AppAttachTask::TaskFunc&& task,
                                               bool isSupportMultiTask) {
    AM_PROFILER_BEGIN();
    int pid = mAppInfo.getAttachingAppPid(prcocessName);
    if (pid < 0) {
        pid = mAppSpawn.appSpawn(execfile.c_str(), {packageName});
        if (pid > 0) {
            mAppInfo.addAppWaitingAttach(prcocessName, pid);
        } else {
            ALOGE("appSpawn App:%s error", execfile.c_str());
            AM_PROFILER_END();
            return -1;
        }
    } else if (!isSupportMultiTask) {
        ALOGW("the Application:%s[%d] is waitting for attach, please wait a moment before "
              "requesting again",
              packageName.c_str(), pid);
        AM_PROFILER_END();
        return -1;
    }

    mPendTask.commitTask(std::make_shared<AppAttachTask>(pid, task));
    AM_PROFILER_END();
    return 0;
}

int ActivityManagerInner::findSystemTarget(const string& targetAlias,
                                           std::shared_ptr<AppRecord>& app, sp<IBinder>& token) {
    ALOGI("findSystemTarget:%s", targetAlias.c_str());
    ActivityHandler activity = nullptr;
    if (targetAlias == Intent::TARGET_ACTIVITY_TOPRESUME) {
        activity = getTopActivity();
        if (activity) {
            app = activity->getAppRecord();
            token = activity->getToken();
        }
    } else if (targetAlias == Intent::TARGET_APPLICATION_FOREGROUND) {
        if (const auto task = mTaskManager.getManager(StandardMode)->getActiveTask()) {
            activity = task->getRootActivity();
            app = activity->getAppRecord();
            token = android::IInterface::asBinder(app->mAppThread);
        }
    } else if (targetAlias == Intent::TARGET_APPLICATION_HOME) {
        if (const auto task = mTaskManager.getHomeTask()) {
            activity = task->getRootActivity();
            app = activity->getAppRecord();
            token = android::IInterface::asBinder(app->mAppThread);
        }
    }
    if (!activity) {
        ALOGW("can not find system target:%s", targetAlias.c_str());
        return -1;
    }
    return 0;
}

ActivityHandler ActivityManagerInner::getActivity(const sp<IBinder>& token) {
    auto iter = mActivityMap.find(token);
    if (iter != mActivityMap.end()) {
        return iter->second;
    } else {
        return nullptr;
    }
}

inline ITaskManager* ActivityManagerInner::getTaskManager(bool isSystemUI) {
    return isSystemUI ? mTaskManager.getManager(TaskManagerType::SystemUIMode)
                      : mTaskManager.getManager(TaskManagerType::StandardMode);
}

inline ActivityHandler ActivityManagerInner::getTopActivity() {
    // Check if there are any active tasks in SystemUI at first
    auto task = mTaskManager.getManager(SystemUIMode)->getActiveTask();
    if (!task) {
        // then check StandardMode task.
        task = mTaskManager.getManager(StandardMode)->getActiveTask();
    }
    if (task) {
        return task->getTopActivity();
    }
    return nullptr;
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

Status ActivityManagerService::stopApplication(const sp<IBinder>& token, int32_t* ret) {
    *ret = mInner->stopApplication(token);
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

Status ActivityManagerService::postIntent(const Intent& intent, int32_t* ret) {
    *ret = mInner->postIntent(intent);
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
