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

namespace os {
namespace am {

using namespace std;

const std::string& ActivityStack::getTaskTag() const {
    return mTag;
}

const int ActivityStack::getSize() const {
    return mTask.size();
}

void ActivityStack::pushActivity(const ActivityHandler& activity) {
    mTask.push_back(activity);
}

ActivityHandler ActivityStack::getTopActivity() {
    if (!mTask.empty()) {
        return mTask.back();
    }
    return nullptr;
}

ActivityHandler ActivityStack::findActivity(const string& activityName) {
    for (auto it : mTask) {
        if (it->mActivityName == activityName) {
            return it;
        }
    }
    return nullptr;
}

void ActivityStack::popToActivity(const ActivityHandler& target) {
    for (auto it = mTask.rbegin(); it != mTask.rend(); ++it) {
        if (*it == target) {
            break;
        }
        /** call Activity Destroy */
        it->get()->destroy();
        mTask.pop_back();
    }
}

void ActivityStack::popAll() {
    for (auto it = mTask.rbegin(); it != mTask.rend(); ++it) {
        /** call Activity Destroy */
        it->get()->destroy();
        mTask.pop_back();
    }
}

/*
 * TaskStackManager: Manage all tasks
 */
TaskHandler TaskStackManager::getActiveTask() {
    return mAllTasks.front();
}

TaskHandler TaskStackManager::findTask(const std::string& tag) {
    for (auto t : mAllTasks) {
        if (t->getTaskTag() == tag) {
            return t;
        }
    }
    return nullptr;
}

void TaskStackManager::initHomeTask(const TaskHandler& task) {
    mAllTasks.emplace_front(task);
    mHomeIter = mAllTasks.begin();
}

void TaskStackManager::pushHomeTaskToFront() {
    // TODO
}

void TaskStackManager::pushActiveTask() {
    // TODO
}

void TaskStackManager::switchTaskToActive() {
    // TODO
}

void TaskStackManager::popFrontTask() {
    // TODO
}

} // namespace am
} // namespace os
