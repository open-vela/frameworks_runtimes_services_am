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

#include "ActivityStack.h"

#include "AppRecord.h"

namespace os {
namespace am {

using namespace std;

const std::string& ActivityStack::getTaskTag() const {
    return mTag;
}

const int ActivityStack::getSize() const {
    return mStack.size();
}

void ActivityStack::pushActivity(const ActivityHandler& activity) {
    mStack.push_back(activity);
}

void ActivityStack::popActivity() {
    mStack.pop_back();
}

void ActivityStack::removeActivity(const ActivityHandler& activity) {
    const int size = mStack.size();
    for (int i = 0; i < size; ++i) {
        if (mStack[i] == activity) {
            mStack[i] = mStack[size - 1];
            mStack.pop_back();
            break;
        }
    }
}

ActivityHandler ActivityStack::getTopActivity() {
    return mStack.empty() ? nullptr : mStack.back();
}

ActivityHandler ActivityStack::getRootActivity() {
    return mStack.empty() ? nullptr : mStack.front();
}

ActivityHandler ActivityStack::findActivity(const string& name) {
    for (const auto& it : mStack) {
        if (it->getName() == name) {
            return it;
        }
    }
    return nullptr;
}

ActivityHandler ActivityStack::findActivity(const sp<IBinder>& token) {
    for (const auto& it : mStack) {
        if (it->getToken() == token) {
            return it;
        }
    }
    return nullptr;
}

void ActivityStack::setForeground(const bool isForeground) {
    for (const auto& it : mStack) {
        if (auto app = it->getAppRecord()) {
            app->setForeground(isForeground);
        }
    }
}

std::ostream& operator<<(std::ostream& os, const ActivityStack& activityStack) {
    os << "Tag{" << activityStack.mTag << "}: ";
    for (auto it = activityStack.mStack.rbegin(); it != activityStack.mStack.rend(); ++it) {
        os << "\n\t" << *(it->get());
    }
    return os;
}

} // namespace am
} // namespace os
