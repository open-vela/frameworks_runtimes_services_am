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
#pragma once

#include <memory>

#include "ActivityStack.h"
#include "AppRecord.h"

namespace os {
namespace am {

using ActivityStackHandler = std::shared_ptr<ActivityStack>;

enum TaskManagerType {
    StandardMode = 0,
    SystemUIMode,
    TYPE_NUM,
};

enum TaskManagerEvent {
    StartActivityEvent,
};

class ITaskManager {
public:
    ITaskManager() = default;
    virtual ~ITaskManager() {}

    virtual void switchTaskToActive(const ActivityStackHandler& task, const Intent& intent) {}
    virtual bool moveTaskToBackground(const ActivityStackHandler& task) {
        return true;
    }
    virtual void pushNewActivity(const ActivityStackHandler& task, const ActivityHandler& activity,
                                 int startFlag) {}
    virtual void turnToActivity(const ActivityStackHandler& task, const ActivityHandler& activity,
                                const Intent& intent, int startFlag) {}
    virtual void finishActivity(const ActivityHandler& activity) {}
    virtual void deleteActivity(const ActivityHandler& activity) {}

    virtual ActivityStackHandler getActiveTask() {
        return nullptr;
    }
    virtual ActivityStackHandler findTask(const std::string& tag) {
        return nullptr;
    }

    virtual void onEvent(TaskManagerEvent event, void* data = nullptr){};
    virtual std::ostream& print(std::ostream& os) {
        return os;
    }
};

class TaskManagerFactory {
public:
    TaskManagerFactory() = default;
    ~TaskManagerFactory() = default;

    bool init(TaskBoard& taskBoard);
    ITaskManager* getManager(TaskManagerType type);
    ActivityStackHandler getHomeTask();
    void onEvent(TaskManagerEvent event, void* data = nullptr);

    friend std::ostream& operator<<(std::ostream& os, const TaskManagerFactory& task);

private:
    std::vector<std::unique_ptr<ITaskManager>> mTaskManagers;
};

} // namespace am
} // namespace os