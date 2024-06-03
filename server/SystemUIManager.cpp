/*
 * Copyright (C) 2024 Xiaomi Corporation
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

#include "SystemUIManager.h"

namespace os {
namespace am {

void SystemUIManager::switchTaskToActive(const ActivityStackHandler& task, const Intent& intent) {
    // all Activity to resume in the task
    for (auto& activity : task->getActivityArray()) {
        if ((intent.mFlag & Intent::FLAG_APP_SWITCH_TASK) != Intent::FLAG_APP_SWITCH_TASK) {
            activity->setIntent(intent);
        }
        activity->lifecycleTransition(ActivityRecord::RESUMED);
        activity->getAppRecord()->setForeground(true);
    }
}

bool SystemUIManager::moveTaskToBackground(const ActivityStackHandler& task) {
    for (auto& activity : task->getActivityArray()) {
        activity->lifecycleTransition(ActivityRecord::STOPPED);
        activity->getAppRecord()->setForeground(false);
    }
    return true;
}

void SystemUIManager::pushNewActivity(const ActivityStackHandler& task,
                                      const ActivityHandler& activity, int startFlag) {
    activity->lifecycleTransition(ActivityRecord::RESUMED);
    activity->getAppRecord()->setForeground(true);

    task->pushActivity(activity);
    if (!findTask(task->getTaskTag())) {
        mSystemUITasks.push_front(task);
    }
}

void SystemUIManager::turnToActivity(const ActivityStackHandler& task,
                                     const ActivityHandler& activity, const Intent& intent,
                                     int startFlag) {
    activity->lifecycleTransition(ActivityRecord::RESUMED);
    activity->getAppRecord()->setForeground(true);
}

void SystemUIManager::finishActivity(const ActivityHandler& activity) {
    activity->lifecycleTransition(ActivityRecord::DESTROYED);
    activity->getAppRecord()->setForeground(false);
}

void SystemUIManager::deleteActivity(const ActivityHandler& activity) {
    auto task = activity->getTask();
    if (task) {
        task->removeActivity(activity);
        if (task->getSize() == 0) {
            mSystemUITasks.remove(task);
        }
    }
}

ActivityStackHandler SystemUIManager::getActiveTask() {
    for (auto& task : mSystemUITasks) {
        for (auto activity : task->getActivityArray()) {
            if (activity->getStatus() == ActivityRecord::RESUMED) {
                return task;
            }
        }
    }
    return nullptr;
}

ActivityStackHandler SystemUIManager::findTask(const std::string& tag) {
    for (const auto& t : mSystemUITasks) {
        if (t->getTaskTag() == tag) {
            // check the taskStack is alive
            if (auto activity = t->getRootActivity()) {
                if (auto app = activity->getAppRecord()) {
                    if (app->mStatus == APP_RUNNING) {
                        return t;
                    }
                }
            }
        }
    }
    return nullptr;
}

void SystemUIManager::onEvent(TaskManagerEvent event, void* data) {
    switch (event) {
        case TaskManagerEvent::StartActivityEvent: {
            onStartActivity();
            break;
        }
    }
}

void SystemUIManager::onStartActivity() {
    for (auto& task : mSystemUITasks) {
        for (auto& activity : task->getActivityArray()) {
            activity->lifecycleTransition(ActivityRecord::STOPPED);
            activity->getAppRecord()->setForeground(false);
        }
    }
}

std::ostream& SystemUIManager::print(std::ostream& os) {
#define RESET "\033[0m"
#define RED "\033[31m"

    if (!mSystemUITasks.empty()) {
        os << RED << "SystemUI task:" << RESET << std::endl;
        for (auto& it : mSystemUITasks) {
            os << *it << std::endl;
        }
    }

    return os;
}

} // namespace am
} // namespace os