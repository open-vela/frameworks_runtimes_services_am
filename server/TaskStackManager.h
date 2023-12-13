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

#include "ActivityStack.h"
#include "AppRecord.h"
#include "TaskBoard.h"

namespace os {
namespace am {

using ActivityStackHandler = std::shared_ptr<ActivityStack>;

/**
 * task list:
 * front-|active|---|foreground task|---|home task|---|background task|-back
 */
class TaskStackManager {
public:
    TaskStackManager(TaskBoard& taskBoard) : mPendTask(taskBoard) {}

    /** Launch the app without Activity name, the intent set to the top Activity. */
    void switchTaskToActive(const ActivityStackHandler& task, const Intent& intent);
    /** Move the Task to the background with the activity's order within the task is unchanged */
    bool moveTaskToBackground(const ActivityStackHandler& task);
    /** must be create new Activity */
    void pushNewActivity(const ActivityStackHandler& task, const ActivityHandler& activity,
                         int startFlag);
    /** turn to the Activity which must exist */
    void turnToActivity(const ActivityStackHandler& task, const ActivityHandler& activity,
                        const Intent& intent, int startFlag);
    /** finish a Activity, set result and resume the last */
    void finishActivity(const ActivityHandler& activity);

    ActivityHandler getActivity(const sp<IBinder>& token);
    void deleteActivity(const ActivityHandler& activity);

    ActivityStackHandler getActiveTask();
    ActivityStackHandler findTask(const std::string& tag);
    void deleteTask(const ActivityStackHandler& task);
    void pushTaskToFront(const ActivityStackHandler& task);

    friend std::ostream& operator<<(std::ostream& os, const TaskStackManager& task);

private:
    std::list<ActivityStackHandler> mAllTasks;
    ActivityStackHandler mHomeTask;
    std::map<sp<IBinder>, ActivityHandler> mActivityMap;
    TaskBoard& mPendTask;
};

} // namespace am
} // namespace os