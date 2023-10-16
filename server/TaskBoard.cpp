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

#include "TaskBoard.h"

namespace os {
namespace am {

TaskBoard::TaskBoard() : mLooper(Looper::getForThread()) {}

void TaskBoard::commitTask(const std::shared_ptr<Task>& task, uint32_t msLimitedTime) {
    const auto taskHandler = sp<TaskMsgHandler>::make(task);
    if (msLimitedTime < UINT_MAX) {
        mLooper->sendMessageDelayed(milliseconds_to_nanoseconds(msLimitedTime), taskHandler, Message());
    }
    mTasklist.emplace_back(taskHandler);
}

void TaskBoard::eventTrigger(const Label& e) {
    for (auto iter = mTasklist.begin(); iter != mTasklist.end();) {
        if ((*iter)->isDone()) {
            /** This situation is handled by timeout. need delete it*/
            auto tmp = iter;
            ++iter;
            mTasklist.erase(tmp);
            continue;
        }
        if (*((*iter)->getTask()) == e) {
            (*iter)->doing(e);
            // execute finish,
            // remove it from looper
            mLooper->removeMessages(*iter);
            // remove it from list
            mTasklist.erase(iter);
            break;
        }
        ++iter;
    }
}

} // namespace am
} // namespace os