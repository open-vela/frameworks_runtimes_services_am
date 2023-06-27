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

namespace os {
namespace am {

using namespace os::app;
using namespace std;

using android::IBinder;
using android::sp;

class ActivityManagerInner {
public:
    int attachApplication(const sp<IApplicationThread>& app);
    int startActivity(const sp<IBinder>& token, const Intent& intent, int32_t requestCode);
    int finishActivity(const sp<IBinder>& token);
    void returnActivityResult(const sp<IBinder>& token, int32_t resultCode, const Intent& data);
    void reportActivityStatus(const sp<IBinder>& token, int32_t status);
    int startService(const sp<IBinder>& token, const Intent& intent);
    void systemReady();

private:
    ActivityHandler getActivityRecord(const sp<IBinder>& token);

private:
    map<sp<IBinder>, ActivityHandler> mActivityMap;
    AppInfoList mAppInfo;
    TaskStackManager mTaskManager;
    IntentAction mActionFilter;
    TaskBoard mPendTask;
};

int ActivityManagerInner::attachApplication(const sp<IApplicationThread>& app) {
    ALOGD("attachApplication");
    return 0;
}

int ActivityManagerInner::startActivity(const sp<IBinder>& caller, const Intent& intent,
                                        int32_t requestCode) {
    ALOGD("startActivity");
    return 0;
}

int ActivityManagerInner::finishActivity(const sp<IBinder>& token) {
    ALOGD("ActivityManager finishActivity");
    // TODO
    return 0;
}

void ActivityManagerInner::returnActivityResult(const sp<IBinder>& token, int32_t resultCode,
                                                const Intent& data) {
    ALOGD("returnActivityResult");
    // TODO
    return;
}

void ActivityManagerInner::reportActivityStatus(const sp<IBinder>& token, int32_t status) {
    ALOGW("reportActivityStatus %d", status);
    // TODO
    return;
}

int ActivityManagerInner::startService(const sp<IBinder>& token, const Intent& intent) {
    ALOGD("startService");
    // TODO
    return 0;
}

void ActivityManagerInner::systemReady() {
    ALOGD("### systemReady ### ");
    AppSpawn::signalInit([](int pid) { ALOGW("AppSpawn pid:%d had exit", pid); });
    // TODO start launch Application
    return;
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

Status ActivityManagerService::finishActivity(const sp<IBinder>& token, int32_t* ret) {
    *ret = mInner->finishActivity(token);
    return Status::ok();
}

Status ActivityManagerService::returnActivityResult(const sp<IBinder>& token, int32_t resultCode,
                                                    const Intent& data) {
    mInner->returnActivityResult(token, resultCode, data);
    return Status::ok();
}

Status ActivityManagerService::reportActivityStatus(const sp<IBinder>& token, int32_t status) {
    mInner->reportActivityStatus(token, status);
    return Status::ok();
}

Status ActivityManagerService::startService(const sp<IBinder>& token, const Intent& intent,
                                            int32_t* ret) {
    return Status::ok();
}

/** The service is ready to start and the application can be launched */
void ActivityManagerService::systemReady() {
    mInner->systemReady();
}

} // namespace am
} // namespace os