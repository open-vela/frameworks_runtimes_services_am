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
#include "TaskManager.h"

#include "SystemUIManager.h"
#include "TaskStackManager.h"

namespace os {
namespace am {

bool TaskManagerFactory::init(TaskBoard& taskBoard) {
    mTaskManagers[StandardMode] = std::make_unique<TaskStackManager>(taskBoard);
    mTaskManagers[SystemUIMode] = std::make_unique<SystemUIManager>();
    return true;
}

ITaskManager* TaskManagerFactory::getManager(TaskManagerType type) {
    ALOG_ASSERT(type >= 0 && type < TYPE_NUM, "the TaskManagerType=%d is invalid!!!", type);
    return mTaskManagers[type].get();
}

ActivityStackHandler TaskManagerFactory::getHomeTask() {
    return ((TaskStackManager*)mTaskManagers[StandardMode].get())->getHomeTask();
}

void TaskManagerFactory::onEvent(TaskManagerEvent event, void* data) {
    for (auto& it : mTaskManagers) {
        it->onEvent(event, data);
    }
}

std::ostream& operator<<(std::ostream& os, const TaskManagerFactory& taskmanager) {
    for (auto& it : taskmanager.mTaskManagers) {
        it->print(os);
    }
    return os;
}

} // namespace am
} // namespace os