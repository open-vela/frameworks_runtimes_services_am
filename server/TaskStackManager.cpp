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

#include <utils/Log.h>

namespace os {
namespace am {

using namespace std;

/*****************************************************
 * TaskStackManager: Manage all Activity task stack
 *****************************************************/

void TaskStackManager::initHomeTask(const ActivityStackHandler& taskStack,
                                    const ActivityHandler& activity) {
    ALOGI("initHomeTask activity:%s", activity->getName().c_str());
    mAllTasks.emplace_front(taskStack);
    mHomeTask = taskStack;
    taskStack->pushActivity(activity);
    mActivityMap.emplace(activity->getToken(), activity);
    activity->create();
    ActivityLifecycleTransition(activity, ActivityRecord::RESUMED);
    mHomeTask->setForeground(true);
}

void TaskStackManager::switchTaskToActive(const ActivityStackHandler& targetStack,
                                          const Intent& intent) {
    ALOGI("switchTaskToActive taskTag:%s", targetStack->getTaskTag().c_str());
    if (targetStack != getActiveTask()) {
        auto currentTopActivity = getActiveTask()->getTopActivity();
        ActivityLifecycleTransition(currentTopActivity, ActivityRecord::PAUSED);

        auto activity = targetStack->getTopActivity();
        activity->setIntent(intent);
        ActivityLifecycleTransition(activity, ActivityRecord::RESUMED);

        auto task = std::make_shared<ActivityWaitResume>(activity, currentTopActivity, this);
        mPendTask.commitTask(task, REQUEST_TIMEOUT_MS);

        pushTaskToFront(targetStack);
    }
}

void TaskStackManager::pushNewActivity(const ActivityStackHandler& targetStack,
                                       const ActivityHandler& activity, int startFlag) {
    ALOGI("pushNewActivity %s flag-cleartask:%d", activity->getName().c_str(),
          startFlag & Intent::FLAG_ACTIVITY_CLEAR_TASK);
    auto currentTopActivity = getActiveTask()->getTopActivity();
    currentTopActivity->pause();

    if (startFlag & Intent::FLAG_ACTIVITY_CLEAR_TASK) {
        while (auto tmpActivity = targetStack->getTopActivity()) {
            ActivityLifecycleTransition(tmpActivity, ActivityRecord::DESTROYED);
            targetStack->popActivity();
        }
    }
    targetStack->pushActivity(activity);
    mActivityMap.emplace(activity->getToken(), activity);
    activity->create();
    ActivityLifecycleTransition(activity, ActivityRecord::RESUMED);

    const auto task = std::make_shared<ActivityWaitResume>(activity, currentTopActivity, this);
    mPendTask.commitTask(task, REQUEST_TIMEOUT_MS);

    pushTaskToFront(targetStack);
}

void TaskStackManager::turnToActivity(const ActivityStackHandler& targetStack,
                                      const ActivityHandler& activity, const Intent& intent,
                                      int startFlag) {
    ALOGI("turnToActivity %s flag-cleartop:%d", activity->getName().c_str(),
          startFlag & Intent::FLAG_ACTIVITY_CLEAR_TOP);
    auto currentTopActivity = getActiveTask()->getTopActivity();
    if (currentTopActivity == activity) {
        currentTopActivity->setIntent(intent);
        /** set pause is a tip, let resume(fake pause)->restart->resume */
        currentTopActivity->setStatus(ActivityRecord::PAUSED);
        ActivityLifecycleTransition(currentTopActivity, ActivityRecord::RESUMED);
    } else {
        if (startFlag & Intent::FLAG_ACTIVITY_CLEAR_TOP) {
            while (auto tmpActivity = targetStack->getTopActivity()) {
                if (tmpActivity == activity) {
                    break;
                }
                ActivityLifecycleTransition(tmpActivity, ActivityRecord::DESTROYED);
                targetStack->popActivity();
            }
        }
        activity->setIntent(intent);
        ActivityLifecycleTransition(activity, ActivityRecord::RESUMED);

        const auto task = std::make_shared<ActivityWaitResume>(activity, currentTopActivity, this);
        mPendTask.commitTask(task, REQUEST_TIMEOUT_MS);

        pushTaskToFront(targetStack);
    }
}

void TaskStackManager::finishActivity(const ActivityHandler& activity) {
    ALOGI("finishActivity %s token:[%p] ", activity->getName().c_str(), activity->getToken().get());
    auto activityTask = activity->getTask();
    if (activity != activityTask->getTopActivity()) {
        while (auto tmpActivity = activityTask->getTopActivity()) {
            if (tmpActivity == activity) {
                break;
            }
            ActivityLifecycleTransition(tmpActivity, ActivityRecord::DESTROYED);
            activityTask->popActivity();
        }
    }

    ActivityLifecycleTransition(activity, ActivityRecord::DESTROYED);
    activityTask->popActivity();

    if (activityTask == getActiveTask()) {
        auto nextActivity = activityTask->getTopActivity();
        if (!nextActivity) {
            mAllTasks.pop_front();
            nextActivity = getActiveTask()->getTopActivity();
        }
        ActivityLifecycleTransition(nextActivity, ActivityRecord::RESUMED);
    }
}

void TaskStackManager::ActivityLifecycleTransition(const ActivityHandler& activity,
                                                   ActivityRecord::Status toStatus) {
    enum { NONE, CREATE = 0, START, RESUME, PAUSE, STOP, DESTROY };
    static int lifeCycleTable[6][6] = {
            /*create*/ {NONE, START, START, START, START, DESTROY},
            /*start*/ {NONE, NONE, RESUME, PAUSE, STOP, STOP},
            /*resume*/ {NONE, START, NONE, PAUSE, PAUSE, PAUSE},
            /*pause*/ {NONE, START, RESUME, NONE, STOP, STOP},
            /*stop*/ {NONE, START, START, NONE, NONE, DESTROY},
            /*destroy*/ {NONE, NONE, NONE, NONE, NONE, NONE},
    };

    const int curStatus = activity->getStatus();
    const int turnTo = lifeCycleTable[curStatus >> 1][toStatus >> 1];

    switch (turnTo) {
        case START:
            activity->start();
            break;
        case RESUME:
            activity->resume();
            break;
        case PAUSE:
            activity->pause();
            break;
        case STOP:
            activity->stop();
            break;
        case DESTROY:
            activity->destroy();
            break;
        case NONE:
            ALOGD("ActivityLifecycleTransition %s[%s] done", activity->getName().c_str(),
                  activity->getStatusStr());
            return;
    }

    ALOGD("ActivityLifecycleTransition %s  [%s] to [%s]", activity->getName().c_str(),
          activity->getStatusStr(), ActivityRecord::statusToStr(toStatus));

    const auto task = std::make_shared<ActivityLifeCycleTask>(activity, this, toStatus);
    mPendTask.commitTask(task, REQUEST_TIMEOUT_MS);
}

ActivityHandler TaskStackManager::getActivity(const sp<IBinder>& token) {
    auto iter = mActivityMap.find(token);
    if (iter != mActivityMap.end()) {
        return iter->second;
    } else {
        return nullptr;
    }
}

void TaskStackManager::deleteActivity(const ActivityHandler& activity) {
    if (auto task = activity->getTask()) {
        if (task->findActivity(activity->getToken())) {
            while (auto tmpActivity = task->getTopActivity()) {
                ActivityLifecycleTransition(tmpActivity, ActivityRecord::DESTROYED);
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
                nextActivity = getActiveTask()->getTopActivity();
            }
            ActivityLifecycleTransition(nextActivity, ActivityRecord::RESUMED);
        } else if (task->getSize() == 0) {
            deleteTask(task);
        }
    }
    mActivityMap.erase(activity->getToken());
}

ActivityStackHandler TaskStackManager::getActiveTask() {
    return mAllTasks.front();
}

ActivityStackHandler TaskStackManager::findTask(const std::string& tag) {
    for (const auto& t : mAllTasks) {
        if (t->getTaskTag() == tag) {
            return t;
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
    if (activityStack != mAllTasks.front()) {
        mAllTasks.remove(activityStack);
        mAllTasks.push_front(activityStack);
    }

    /** TaskStack switching, modifies foreground and background applications */
    if (activityStack != mHomeTask) {
        activityStack->setForeground(true);
    } else {
        for (auto iter = ++mAllTasks.begin(); iter != mAllTasks.end(); ++iter) {
            (*iter)->setForeground(false);
        }
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

} // namespace am
} // namespace os