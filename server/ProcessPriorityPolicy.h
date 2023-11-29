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

#include <sys/types.h>
#include <time.h>

#include <iostream>
#include <list>
#include <unordered_map>

#include "LowMemoryManager.h"

namespace os {
namespace am {

enum PriorityLevel {
    PERSISTENT,
    HIGH,
    MIDDLE,
    LOW,
};

enum OomScoreAdj {
    OS_SYSTEM_ADJ = -900,
    OS_PERSISTENT_PROC_ADJ = -100,
    OS_FOREGROUND_APP_ADJ = 0,
    OS_SYSTEM_HOME_APP_ADJ = 1,
    OS_HIGH_LEVEL_MIN_ADJ = 10,
    OS_HIGH_LEVEL_MAX_ADJ = 99,
    OS_MIDDLE_LEVEL_MIN_ADJ = 100,
    OS_MIDDLE_LEVEL_MAX_ADJ = 600,
    OS_LOW_LEVEL_MIN_ADJ = 700,
    OS_LOW_LEVEL_MAX_ADJ = 800,
    OS_CACHE_PROCESS_ADJ = 900,
};

struct PidPriorityInfo {
    pid_t pid;
    PriorityLevel priorityLevel;
    int oomScore;
    clock_t lastWakeUptime;

    PidPriorityInfo* next;
    PidPriorityInfo* last;
};

/*********************************************
 *
 * <node>---<node>---<node>---<node>
 *   ^     home app     ^      tail
 *  head          mBackgroundPos
 *
 *********************************************/
class ProcessPriorityPolicy {
public:
    ProcessPriorityPolicy(LowMemoryManager* lmk);

    PidPriorityInfo* get(pid_t pid);
    PidPriorityInfo* add(pid_t pid, bool isForeground, PriorityLevel level = MIDDLE);
    void remove(pid_t pid);
    void pushForeground(pid_t pid);
    void intoBackground(pid_t pid);

    void analyseProcessPriority();

    friend std::ostream& operator<<(std::ostream& os, ProcessPriorityPolicy& policy);

private:
    LowMemoryManager* mLmk;
    PidPriorityInfo* mHead;
    PidPriorityInfo* mTail;
    PidPriorityInfo* mBackgroundPos;
};

} // namespace am
} // namespace os