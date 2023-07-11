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

class ActivityManagerInner {
public:
    int attachApplication(const sp<IApplicationThread>& app);
    int startActivity(const sp<IBinder>& token, const Intent& intent, int32_t requestCode);
    bool finishActivity(const sp<IBinder>& token, int32_t resultCode,
                        const std::optional<Intent>& resultData);
    void reportActivityStatus(const sp<IBinder>& token, int32_t status);
    int startService(const Intent& intent);
    int stopService(const Intent& intent);
    void reportServiceStatus(const string& target, int32_t status);

    void systemReady();

private:
    ActivityHandler getActivityRecord(const sp<IBinder>& token);
    ActivityHandler startActivityReal(const std::shared_ptr<AppRecord>& app,
                                      const ActivityInfo& activityName, const Intent& intent,
                                      const sp<IBinder>& caller, const int32_t requestCode);
    int startHomeActivity();

private:
    map<sp<IBinder>, ActivityHandler> mActivityMap;
    AppInfoList mAppInfo;
    TaskStackManager mTaskManager;
    IntentAction mActionFilter;
    TaskBoard mPendTask;
    PackageManager mPm;
};

int ActivityManagerInner::attachApplication(const sp<IApplicationThread>& app) {
    ALOGD("attachApplication");
    const int callerPid = android::IPCThreadState::self()->getCallingPid();
    const auto appRecord = mAppInfo.findAppInfo(callerPid);
    if (appRecord) {
        ALOGE("the application:%s had be attached", appRecord->mPackageName.c_str());
        return android::BAD_VALUE;
    }

    const int callerUid = android::IPCThreadState::self()->getCallingUid();
    const AppAttachTask::Event event(callerPid, callerUid, app);
    mPendTask.eventTrigger(&event);

    return android::OK;
}

int ActivityManagerInner::startActivity(const sp<IBinder>& caller, const Intent& intent,
                                        int32_t requestCode) {
    ALOGD("startActivity");
    auto topActivity = mTaskManager.getActiveTask()->getTopActivity();
    if (topActivity->mStatus != ActivityRecord::RESUMED) {
        /** The currently active Activity is undergoing a state transition, it is't accepting
         * other startActivity requests */
        ALOGW("the top Activity Status is changing");
        return android::INVALID_OPERATION;
    }

    if (caller != topActivity->mToken && !(intent.mFlag & Intent::FLAG_ACTIVITY_NEW_TASK)) {
        /** if the current caller is not the top Activity */
        ALOGW("Inappropriate, the caller must startActivity in other task");
        return android::PERMISSION_DENIED;
    }

    string activityTarget;
    if (!intent.mTarget.empty()) {
        activityTarget = intent.mTarget;
    } else {
        if (!mActionFilter.getFirstTargetByAction(intent.mAction, activityTarget)) {
            ALOGE("can't find the Activity by action:%s", intent.mAction.c_str());
            return android::BAD_VALUE;
        }
    }
    ALOGI("start activity:%s intent:%s %s flag:%d", activityTarget.c_str(), intent.mAction.c_str(),
          intent.mData.c_str(), intent.mFlag);

    /** get Package and Activity info for PMS */
    const auto packagePos = activityTarget.find_first_of('/');
    const string packageName = activityTarget.substr(0, packagePos);
    PackageInfo packageInfo;
    if (mPm.getPackageInfo(packageName, &packageInfo)) {
        ALOGE("error packagename:%s", packageName.c_str());
        return android::BAD_VALUE;
    }

    const string activityName = packagePos != string::npos
            ? activityTarget.substr(packagePos + 1, string::npos)
            : packageInfo.entry;

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
        /** get Application thread */
        auto activityRecord = startActivityReal(appInfo, activityInfo, intent, caller, requestCode);
        /** When the target Activity has been resumed. then stop the previous Activity */
        const auto task = std::make_shared<ActivityResumeTask>(activityRecord->mToken,
                                                               [this, topActivity]() -> bool {
                                                                   topActivity->stop();
                                                                   return true;
                                                               });
        mPendTask.commitTask(task);
    } else {
        /** The app hasn't started yet */
        int pid;
        if (AppSpawn::appSpawn(&pid, packageInfo.execfile.c_str(), NULL) == 0) {
            auto task = std::make_shared<
                    AppAttachTask>(pid,
                                   [this, packageName, activityInfo, intent, caller,
                                    requestCode](const AppAttachTask::Event* e) -> bool {
                                       auto appRecord =
                                               std::make_shared<AppRecord>(e->mAppHandler,
                                                                           packageName, e->mPid,
                                                                           e->mUid);
                                       this->mAppInfo.addAppInfo(appRecord);
                                       this->startActivityReal(appRecord, activityInfo, intent,
                                                               caller, requestCode);
                                       return true;
                                   });
            mPendTask.commitTask(task);
        } else {
            /** Failed to start, restore the previous pause activity */
            topActivity->resume();
        }
    }

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
    }

    bool isCreateActivity = true;
    ActivityHandler record;
    if (!isCreateTask) {
        if (intent.mFlag == Intent::NO_FLAG) {
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
    }

    if (isCreateActivity) {
        sp<IBinder> token(new android::BBinder());
        record = std::make_shared<ActivityRecord>(activityInfo.name, token, caller, requestCode,
                                                  activityInfo.launchMode, app, targetTask);
        mActivityMap.emplace(token, record);
        targetTask->pushActivity(record);
        app->mAppThread->scheduleLaunchActivity(activityInfo.name, token, intent);
    } else {
        record->mIntent = intent;
        record->resume();
    }

    return record;
}

bool ActivityManagerInner::finishActivity(const sp<IBinder>& token, int32_t resultCode,
                                          const std::optional<Intent>& resultData) {
    ALOGD("finishActivity");
    auto currentActivity = getActivityRecord(token);
    if (!currentActivity) {
        ALOGE("finishActivity: The token is invalid");
        return false;
    }
    if (currentActivity->mStatus == ActivityRecord::RESUMED) {
        currentActivity->pause();

        const auto task = [this, currentActivity, resultCode, resultData]() -> bool {
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
            if (nextActivity) {
                nextActivity->resume();
            } else {
                /** nextActivity is null, the ActivityStack should be
                 * destory */
                mTaskManager.popFrontTask();
                const auto activeTask = mTaskManager.getActiveTask();
                nextActivity = activeTask->getTopActivity();
                nextActivity->resume();
            }

            const auto tmpTask = [this, currentActivity]() -> bool {
                currentActivity->stop();
                currentActivity->destroy();
                // TODO when report destroy. delete it from mActivityMap
                return true;
            };
            const auto destoryActivityTask =
                    std::make_shared<ActivityResumeTask>(nextActivity->mToken, tmpTask);
            mPendTask.commitTask(destoryActivityTask);

            return true;
        };
        const auto finishActivityTask = std::make_shared<ActivityPauseTask>(token, task);

        mPendTask.commitTask(finishActivityTask);
    } else {
        ALOGE("the Activity that being finished is inactive !!!");
        currentActivity->stop();
        currentActivity->destroy();
    }

    return true;
}

void ActivityManagerInner::reportActivityStatus(const sp<IBinder>& token, int32_t status) {
    ALOGD("reportActivityStatus %d", status);
    auto record = getActivityRecord(token);
    if (!record) {
        ALOGE("The reported token is invalid");
        return;
    }
    record->mStatus = status;
    switch (status) {
        case ActivityRecord::CREATED:
        case ActivityRecord::STARTED:
            break;
        case ActivityRecord::RESUMED: {
            const ActivityResumeTask::Event event(token);
            mPendTask.eventTrigger(&event);
            break;
        }
        case ActivityRecord::PAUSED: {
            const ActivityPauseTask::Event event(token);
            mPendTask.eventTrigger(&event);
            break;
        }
        case ActivityRecord::STOPED:
        case ActivityRecord::DESTROYED:
        default:
            break;
    }
    return;
}

int ActivityManagerInner::startService(const Intent& intent) {
    // TODO
    return android::OK;
}

int ActivityManagerInner::stopService(const Intent& intent) {
    // TODO
    return 0;
}

void ActivityManagerInner::reportServiceStatus(const string& target, int32_t status) {
    // TODO
}

void ActivityManagerInner::systemReady() {
    ALOGD("### systemReady ### ");
    AppSpawn::signalInit([](int pid) { ALOGW("AppSpawn pid:%d had exit", pid); });
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
    return;
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
        int pid;
        if (AppSpawn::appSpawn(&pid, homeApp.execfile.c_str(), NULL) == 0) {
            auto task = std::make_shared<
                    AppAttachTask>(pid,
                                   [this, packageName,
                                    entryActivity](const AppAttachTask::Event* e) -> bool {
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
                                       return true;
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

Status ActivityManagerService::reportServiceStatus(const string& target, int32_t status) {
    mInner->reportServiceStatus(target, status);
    return Status::ok();
}

/** The service is ready to start and the application can be launched */
void ActivityManagerService::systemReady() {
    mInner->systemReady();
}

} // namespace am
} // namespace os