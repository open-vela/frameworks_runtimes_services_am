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

#include "AppRecord.h"

namespace os {
namespace am {

using namespace std;

ActivityHandler AppRecord::checkActivity(const std::string& activityName) {
    const int size = mExistActivity.size();
    for (int i = 0; i < size; ++i) {
        if (mExistActivity[i].lock()->getName() == activityName) {
            return mExistActivity[i].lock();
        }
    }
    return nullptr;
}

ServiceHandler AppRecord::checkService(const std::string& serviceName) {
    const int size = mExistService.size();
    for (int i = 0; i < size; ++i) {
        if (mExistService[i].lock()->mServiceName == serviceName) {
            return mExistService[i].lock();
        }
    }
    return nullptr;
}

void AppRecord::addActivity(const std::shared_ptr<ActivityRecord>& activity) {
    mExistActivity.emplace_back(activity);
}

int AppRecord::deleteActivity(const std::shared_ptr<ActivityRecord>& activity) {
    const int size = mExistActivity.size();
    for (int i = 0; i < size; ++i) {
        if (mExistActivity[i].lock() == activity) {
            mExistActivity[i] = mExistActivity[size - 1];
            mExistActivity.pop_back();
            break;
        }
    }
    return 0;
}

void AppRecord::addService(const std::shared_ptr<ServiceRecord>& service) {
    mExistService.emplace_back(service);
}

int AppRecord::deleteService(const std::shared_ptr<ServiceRecord>& service) {
    const int size = mExistService.size();
    for (int i = 0; i < size; ++i) {
        if (mExistService[i].lock() == service) {
            mExistService[i] = mExistService[size - 1];
            mExistService.pop_back();
            break;
        }
    }
    return 0;
}

void AppRecord::setForeground(const bool isForegroundActivity) {
    if (mStatus == APP_STOPPED) {
        return;
    }
    if (isForegroundActivity) {
        if (++mForegroundActivityCnt == 1) {
            mAppThread->setForegroundApplication(true);
            mPriorityPolicy->pushForeground(mPid);
        }
    } else {
        if (--mForegroundActivityCnt == 0) {
            mAppThread->setForegroundApplication(false);
            mPriorityPolicy->intoBackground(mPid);
        }
    }
    ALOGD("%s ForegroundActivityCnt:%d", mPackageName.c_str(), mForegroundActivityCnt);
}

void AppRecord::scheduleReceiveIntent(const sp<IBinder>& token, const Intent& intent) {
    if (mStatus != APP_STOPPED) {
        mAppThread->scheduleReceiveIntent(token, intent);
    }
}

bool AppRecord::checkActiveStatus() const {
    if (mExistActivity.empty() && mExistService.empty()) {
        return false;
    }
    return true;
}

void AppRecord::stopApplication() {
    if (mStatus == APP_RUNNING) {
        mAppThread->terminateApplication();
        mStatus = APP_STOPPING;
    }
}

const shared_ptr<AppRecord> AppInfoList::findAppInfo(const int pid) {
    for (auto it : mAppList) {
        if (it->mPid == pid) {
            return it;
        }
    }
    return nullptr;
}

const shared_ptr<AppRecord> AppInfoList::findAppInfoWithAlive(const int pid) {
    for (auto it : mAppList) {
        if (it->mPid == pid && it->mStatus == APP_RUNNING) {
            return it;
        }
    }
    return nullptr;
}

const shared_ptr<AppRecord> AppInfoList::findAppInfoWithAlive(const string& packageName) {
    for (auto it : mAppList) {
        if (it->mPackageName == packageName && it->mStatus == APP_RUNNING) {
            return it;
        }
    }
    return nullptr;
}

bool AppInfoList::addAppInfo(const shared_ptr<AppRecord>& appInfo) {
    if (nullptr == findAppInfo(appInfo->mPid).get()) {
        mAppList.emplace_back(appInfo);
        return true;
    }
    return false;
}

void AppInfoList::deleteAppInfo(const int pid) {
    const int size = mAppList.size();
    for (int i = 0; i < size; ++i) {
        if (mAppList[i]->mPid == pid) {
            mAppList[i] = mAppList[size - 1];
            mAppList.pop_back();
            break;
        }
    }
}

void AppInfoList::deleteAppInfo(const string& packageName) {
    const int size = mAppList.size();
    for (int i = 0; i < size; ++i) {
        if (mAppList[i]->mPackageName == packageName) {
            mAppList[i] = mAppList[size - 1];
            mAppList.pop_back();
            break;
        }
    }
}

void AppInfoList::addAppWaitingAttach(const std::string& packageName, int pid) {
    mAppWaitingAttach.emplace_back(packageName, pid);
}

void AppInfoList::deleteAppWaitingAttach(const int pid) {
    const int size = mAppWaitingAttach.size();
    for (int i = 0; i < size; ++i) {
        if (mAppWaitingAttach[i].second == pid) {
            mAppWaitingAttach[i] = mAppWaitingAttach[size - 1];
            mAppWaitingAttach.pop_back();
            break;
        }
    }
}

int AppInfoList::getAttachingAppPid(const std::string& packageName) {
    for (auto it : mAppWaitingAttach) {
        if (it.first == packageName) return it.second;
    }
    return -1;
}

bool AppInfoList::getAttachingAppName(int pid, std::string& packageName) {
    for (auto it : mAppWaitingAttach) {
        if (it.second == pid) {
            packageName = it.first;
            return true;
        }
    }
    return false;
}

} // namespace am
} // namespace os
