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

void ActivityStack::popActivity() {
    mTask.pop_back();
}

ActivityHandler ActivityStack::getTopActivity() {
    return mTask.empty() ? nullptr : mTask.back();
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

std::ostream& operator<<(std::ostream& os, const ActivityStack& activityStack) {
    os << "Tag{" << activityStack.mTag << "}: ";
    for (auto it = activityStack.mTask.rbegin(); it != activityStack.mTask.rend(); ++it) {
        os << "\n\t" << *(it->get());
    }
    return os;
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
    mHomeTask = task;
}

void TaskStackManager::pushHomeTaskToFront() {
    mAllTasks.remove(mHomeTask);
    mAllTasks.emplace_front(mHomeTask);
}

void TaskStackManager::pushActiveTask(const TaskHandler& task) {
    mAllTasks.emplace_front(task);
}

void TaskStackManager::switchTaskToActive(const TaskHandler& task) {
    mAllTasks.remove(task);
    mAllTasks.push_front(task);
}

void TaskStackManager::popFrontTask() {
    mAllTasks.pop_front();
}

std::ostream& operator<<(std::ostream& os, const TaskStackManager& task) {
#define RESET "\033[0m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"

    os << GREEN << "foreground task:" << RESET << endl;
    for (auto& it : task.mAllTasks) {
        if (it == task.mHomeTask) {
            os << YELLOW << "home task:" << RESET << endl;
            os << *it << endl;
            os << BLUE << "background task:" << RESET << endl;
        } else {
            os << *it << endl;
        }
    }
    return os;
}

} // namespace am
} // namespace os
