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

#define LOG_TAG "TaskStackManager"

#include "TaskStackManager.h"

#include "app/Logger.h"

namespace os {
namespace am {

using namespace std;

/*****************************************************
 * TaskStackManager: Manage all Activity task stack
 *****************************************************/

void TaskStackManager::switchTaskToActive(const ActivityStackHandler& targetStack,
                                          const Intent& intent) {
    ALOGI("switchTaskToActive taskTag:%s", targetStack->getTaskTag().c_str());
    auto activeTask = getActiveTask();
    if (targetStack != activeTask) {
        auto currentTopActivity = activeTask->getTopActivity();
        currentTopActivity->lifecycleTransition(ActivityRecord::PAUSED);

        auto activity = targetStack->getTopActivity();
        if (activity) {
            activity->setIntent(intent);
            activity->lifecycleTransition(ActivityRecord::RESUMED);
            auto task = std::make_shared<ActivityWaitResume>(activity, currentTopActivity);
            mPendTask.commitTask(task, REQUEST_TIMEOUT_MS);
            pushTaskToFront(targetStack);
        } else {
            ALOGE("switchTaskToActive error:%s the task is empty!!!",
                  targetStack->getTaskTag().c_str());
            deleteTask(targetStack);
        }
    }
}

/** Move the Task to the background with the activity's order within the task is unchanged */
bool TaskStackManager::moveTaskToBackground(const ActivityStackHandler& targetStack) {
    ALOGI("moveTaskToBack taskTag:%s", targetStack->getTaskTag().c_str());
    bool isBeforeHomeTask = false;
    auto activeTask = getActiveTask();
    if (targetStack == mHomeTask) {
        ALOGW("default home application can't move to background");
        return false;
    }

    if (targetStack == activeTask) {
        isBeforeHomeTask = true;
        auto topActivity = targetStack->getTopActivity();
        topActivity->lifecycleTransition(ActivityRecord::PAUSED);
        targetStack->setForeground(false);
        mAllTasks.pop_front();
        activeTask = getActiveTask();
        if (activeTask) {
            auto nextActivity = activeTask->getTopActivity();
            if (nextActivity) {
                nextActivity->lifecycleTransition(ActivityRecord::RESUMED);
                auto task = std::make_shared<ActivityWaitResume>(nextActivity, topActivity);
                mPendTask.commitTask(task, REQUEST_TIMEOUT_MS);
            }
            activeTask->setForeground(true);
        }
    }

    for (auto iter = mAllTasks.begin(); iter != mAllTasks.end();) {
        if (*iter == targetStack) {
            isBeforeHomeTask = true;
            auto tmp = iter;
            ++iter;
            mAllTasks.erase(tmp);
        } else {
            if (*iter == mHomeTask) {
                if (isBeforeHomeTask) {
                    mAllTasks.insert(++iter, targetStack);
                }
                return true;
            }
            ++iter;
        }
    }

    return false;
}

void TaskStackManager::pushNewActivity(const ActivityStackHandler& targetStack,
                                       const ActivityHandler& activity, int startFlag) {
    ALOGI("pushNewActivity %s flag-cleartask:%d", activity->getName().c_str(),
          startFlag & Intent::FLAG_ACTIVITY_CLEAR_TASK);
    ActivityHandler lastTopActivity;
    const auto activeTask = getActiveTask();
    if (activeTask) {
        lastTopActivity = activeTask->getTopActivity();
        lastTopActivity->lifecycleTransition(ActivityRecord::PAUSED);
    } else {
        mHomeTask = targetStack;
    }

    if (startFlag & Intent::FLAG_ACTIVITY_CLEAR_TASK) {
        while (auto tmpActivity = targetStack->getTopActivity()) {
            tmpActivity->lifecycleTransition(ActivityRecord::DESTROYED);

            targetStack->popActivity();
            if (targetStack == activeTask) {
                tmpActivity->getAppRecord()->setForeground(false);
            }
        }
    }
    targetStack->pushActivity(activity);
    if (targetStack == activeTask) {
        activity->getAppRecord()->setForeground(true);
    }
    activity->lifecycleTransition(ActivityRecord::RESUMED);

    if (lastTopActivity) {
        const auto task = std::make_shared<ActivityWaitResume>(activity, lastTopActivity);
        mPendTask.commitTask(task, REQUEST_TIMEOUT_MS);
    }

    pushTaskToFront(targetStack);
}

void TaskStackManager::turnToActivity(const ActivityStackHandler& targetStack,
                                      const ActivityHandler& activity, const Intent& intent,
                                      int startFlag) {
    ALOGI("turnToActivity %s flag-cleartop:%d", activity->getName().c_str(),
          startFlag & Intent::FLAG_ACTIVITY_CLEAR_TOP);
    ActivityHandler lastTopActivity;
    const auto activeTask = getActiveTask();
    if (activeTask) {
        lastTopActivity = activeTask->getTopActivity();
    }
    if (lastTopActivity == activity) {
        activity->setIntent(intent);
        /** set pause is a tip, let resume(fake pause)->restart->resume */
        activity->setStatus(ActivityRecord::PAUSED);
        activity->lifecycleTransition(ActivityRecord::RESUMED);

    } else {
        if (lastTopActivity) {
            lastTopActivity->lifecycleTransition(ActivityRecord::PAUSED);
        }
        if (startFlag & Intent::FLAG_ACTIVITY_CLEAR_TOP) {
            while (auto tmpActivity = targetStack->getTopActivity()) {
                if (tmpActivity == activity) {
                    break;
                }
                tmpActivity->lifecycleTransition(ActivityRecord::DESTROYED);

                targetStack->popActivity();
                if (targetStack == activeTask) {
                    tmpActivity->getAppRecord()->setForeground(false);
                }
            }
        }
        activity->setIntent(intent);
        activity->lifecycleTransition(ActivityRecord::RESUMED);

        if (targetStack != activeTask && lastTopActivity) {
            const auto task = std::make_shared<ActivityWaitResume>(activity, lastTopActivity);
            mPendTask.commitTask(task, REQUEST_TIMEOUT_MS);
        }

        pushTaskToFront(targetStack);
    }
}

void TaskStackManager::finishActivity(const ActivityHandler& activity) {
    ALOGI("finishActivity %s token:[%p] ", activity->getName().c_str(), activity->getToken().get());
    auto activityTask = activity->getTask();
    auto activeTask = getActiveTask();
    if (!activityTask) {
        ALOGW("the TaskStack that Activity:%s belonged to had been removed",
              activity->getName().c_str());
        return;
    }
    if (activity != activityTask->getTopActivity()) {
        while (auto tmpActivity = activityTask->getTopActivity()) {
            if (tmpActivity == activity) {
                break;
            }
            tmpActivity->lifecycleTransition(ActivityRecord::DESTROYED);

            activityTask->popActivity();
            if (activityTask == activeTask) {
                tmpActivity->getAppRecord()->setForeground(false);
            }
        }
    }

    activityTask->popActivity();

    if (activityTask == activeTask) {
        activity->getAppRecord()->setForeground(false);
        auto nextActivity = activityTask->getTopActivity();
        if (!nextActivity) {
            mAllTasks.pop_front();
            activeTask = getActiveTask();
            if (activityTask == mHomeTask) {
                ALOGW("Default desktop application exit!!!");
                mHomeTask = activeTask;
            }
            if (activeTask) {
                nextActivity = activeTask->getTopActivity();
                activeTask->setForeground(true);
            }
        }
        if (nextActivity) {
            activity->lifecycleTransition(ActivityRecord::PAUSED);
            nextActivity->lifecycleTransition(ActivityRecord::RESUMED);
            const auto task = std::make_shared<ActivityDelayDestroy>(activity, nextActivity);
            mPendTask.commitTask(task, REQUEST_TIMEOUT_MS);
        } else {
            activity->lifecycleTransition(ActivityRecord::DESTROYED);
        }
    } else {
        activity->lifecycleTransition(ActivityRecord::DESTROYED);
        if (!activityTask->getTopActivity()) {
            deleteTask(activityTask);
        }
    }
}

void TaskStackManager::deleteActivity(const ActivityHandler& activity) {
    if (auto task = activity->getTask()) {
        if (task->findActivity(activity->getToken())) {
            while (auto tmpActivity = task->getTopActivity()) {
                tmpActivity->lifecycleTransition(ActivityRecord::DESTROYED);
                task->popActivity();
                if (tmpActivity == activity) {
                    break;
                }
            }
        }

        if (task == getActiveTask()) {
            auto nextActivity = task->getTopActivity();
            if (!nextActivity) {
                mAllTasks.pop_front();
                if (auto activeTask = getActiveTask()) {
                    nextActivity = getActiveTask()->getTopActivity();
                }
            }
            if (nextActivity) {
                nextActivity->lifecycleTransition(ActivityRecord::RESUMED);
            }

        } else if (task->getSize() == 0) {
            deleteTask(task);
        }

        if (task == mHomeTask && task->getSize() == 0) {
            ALOGE("Default desktop application exit!!!");
            mHomeTask = getActiveTask();
        }
    }
}

ActivityStackHandler TaskStackManager::getActiveTask() {
    if (!mAllTasks.empty()) {
        return mAllTasks.front();
    }
    return nullptr;
}

ActivityStackHandler TaskStackManager::getHomeTask() {
    return mHomeTask;
}

ActivityStackHandler TaskStackManager::findTask(const std::string& tag) {
    for (const auto& t : mAllTasks) {
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

void TaskStackManager::deleteTask(const ActivityStackHandler& task) {
    for (auto it = mAllTasks.begin(); it != mAllTasks.end(); ++it) {
        if (*it == task) {
            mAllTasks.erase(it);
            break;
        }
    }
}

void TaskStackManager::pushTaskToFront(const ActivityStackHandler& activityStack) {
    const auto activeTask = getActiveTask();
    if (activityStack != activeTask) {
        if (activeTask) {
            activeTask->setForeground(false);
        }
        mAllTasks.remove(activityStack);
        mAllTasks.push_front(activityStack);
        activityStack->setForeground(true);
    }
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

std::ostream& TaskStackManager::print(std::ostream& os) {
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"

    os << RED << "foreground task:" << RESET << endl;
    for (auto& it : mAllTasks) {
        if (it == mHomeTask) {
            os << YELLOW << "home task:" << RESET << endl;
            os << *it << endl;
            os << GREEN << "background task:" << RESET << endl;
        } else {
            os << *it << endl;
        }
    }
    return os;
}

} // namespace am
} // namespace os