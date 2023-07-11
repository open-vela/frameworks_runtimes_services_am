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
        if (mExistActivity[i].lock()->mActivityName == activityName) {
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

const shared_ptr<AppRecord> AppInfoList::findAppInfo(const int pid) {
    const int size = mAppList.size();
    for (int i = 0; i < size; ++i) {
        if (mAppList[i]->mPid == pid) {
            return mAppList[i];
        }
    }
    return nullptr;
}

const shared_ptr<AppRecord> AppInfoList::findAppInfo(const string& packageName) {
    const int size = mAppList.size();
    for (int i = 0; i < size; ++i) {
        if (mAppList[i]->mPackageName == packageName) {
            return mAppList[i];
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

} // namespace am
} // namespace os
