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

#include "ProcessPriorityPolicy.h"

#include <utils/Log.h>

namespace os {
namespace am {

enum ProcessStatus {
    FOREGROUND_PROCESS,
    SYSTEM_HOME_PROCESS,
    BACKGROUND_PROCESS,
};

enum LevelCountMask {
    HIGH_LEVLE_CNT = (0b1111111111 << 20),
    MIDDLE_LEVEL_CNT = (0b1111111111 << 10),
    LOW_LEVEL_CNT = 0b1111111111,
};

static int calculateScore(PidPriorityInfo* pnode, int& levelCnt, ProcessStatus location) {
    int score = 1000;

    if (location == FOREGROUND_PROCESS) {
        score = pnode->oomScore > OS_FOREGROUND_APP_ADJ ? OS_FOREGROUND_APP_ADJ : pnode->oomScore;
        return score;
    } else if (location == SYSTEM_HOME_PROCESS) {
        score = pnode->oomScore > OS_SYSTEM_HOME_APP_ADJ ? OS_SYSTEM_HOME_APP_ADJ : pnode->oomScore;
        return score;
    }

    switch (pnode->priorityLevel) {
        case ProcessPriority::PERSISTENT: {
            score = OS_PERSISTENT_PROC_ADJ;
            break;
        }
        case ProcessPriority::HIGH: {
            score = OS_HIGH_LEVEL_MIN_ADJ + ((levelCnt & HIGH_LEVLE_CNT) >> 20);
            levelCnt += (1 << 20);
            break;
        }
        case ProcessPriority::MIDDLE: {
            score = OS_MIDDLE_LEVEL_MIN_ADJ + ((levelCnt & MIDDLE_LEVEL_CNT) >> 10);
            levelCnt += (1 << 10);
            break;
        }
        case ProcessPriority::LOW: {
            score = OS_LOW_LEVEL_MIN_ADJ + (levelCnt & MIDDLE_LEVEL_CNT);
            levelCnt += 1;
            break;
        }
    }

    return score;
}

ProcessPriorityPolicy::ProcessPriorityPolicy(LowMemoryManager* lmk) {
    mLmk = lmk;
    mHead = nullptr;
    mTail = nullptr;
    mBackgroundPos = nullptr;
    lmk->setPrepareLMKCallback([this] { analyseProcessPriority(); });
}

void ProcessPriorityPolicy::analyseProcessPriority() {
    ALOGD("analyseProcessPriority");
    int levelcnt = 0;
    PidPriorityInfo* pnode = mHead;
    ProcessStatus processStatus = FOREGROUND_PROCESS;
    while (pnode) {
        if (pnode->next == mBackgroundPos) {
            const int score = calculateScore(pnode, levelcnt, SYSTEM_HOME_PROCESS);
            if (pnode->oomScore != score) {
                pnode->oomScore = score;
                mLmk->setPidOomScore(pnode->pid, pnode->oomScore);
            }
            processStatus = BACKGROUND_PROCESS;
        } else {
            const int score = calculateScore(pnode, levelcnt, processStatus);
            if (pnode->oomScore != score) {
                pnode->oomScore = score;
                mLmk->setPidOomScore(pnode->pid, pnode->oomScore);
            }
        }
        pnode = pnode->next;
    }
}

PidPriorityInfo* ProcessPriorityPolicy::get(pid_t pid) {
    PidPriorityInfo* pnode = mHead;
    while (pnode) {
        if (pnode->pid == pid) {
            break;
        }
        pnode = pnode->next;
    }
    return pnode;
}

PidPriorityInfo* ProcessPriorityPolicy::add(pid_t pid, bool isForeground, ProcessPriority level) {
    PidPriorityInfo* pnode = get(pid);
    if (pnode == nullptr) {
        pnode = new PidPriorityInfo{pid, level, OS_MIDDLE_LEVEL_MIN_ADJ, clock(), nullptr, nullptr};
        mLmk->setPidOomScore(pid, OS_MIDDLE_LEVEL_MIN_ADJ); // set default
        if (isForeground) {
            pnode->next = mHead;
            if (mHead) mHead->last = pnode;
            mHead = pnode;
            if (mTail == nullptr) {
                mTail = pnode;
            }
        } else {
            pnode->next = mBackgroundPos;
            if (mBackgroundPos) {
                pnode->last = mBackgroundPos->last;
                mBackgroundPos->last->next = pnode;
                mBackgroundPos->last = pnode;
            } else {
                pnode->last = mTail;
                mTail->next = pnode;
                mTail = pnode;
            }
            mBackgroundPos = pnode;
        }
    }

    return pnode;
}

void ProcessPriorityPolicy::remove(pid_t pid) {
    PidPriorityInfo* pnode = get(pid);
    if (pnode != nullptr) {
        if (mHead == pnode) {
            mHead = pnode->next;
        }
        if (mTail == pnode) {
            mTail = pnode->last;
        }
        if (mBackgroundPos == pnode) {
            mBackgroundPos = pnode->next;
        }

        if (pnode->last) {
            pnode->last->next = pnode->next;
        }
        if (pnode->next) {
            pnode->next->last = pnode->last;
        }

        delete pnode;
    }

    mLmk->cancelMonitorPid(pid);
}

void ProcessPriorityPolicy::pushForeground(pid_t pid) {
    PidPriorityInfo* pnode = get(pid);
    if (pnode) {
        if (mBackgroundPos && pnode == mBackgroundPos->last) {
            mBackgroundPos = mHead;
        }
        if (mBackgroundPos && pnode == mBackgroundPos) {
            mBackgroundPos = pnode->next;
        }
        if (mHead != pnode) {
            if (pnode->next) pnode->next->last = pnode->last;
            if (pnode->last) {
                pnode->last->next = pnode->next;
            }
            if (mTail == pnode && pnode->last) {
                mTail = pnode->last;
            }

            pnode->next = mHead;
            pnode->last = nullptr;
            mHead->last = pnode;
            mHead = pnode;
        }
        pnode->lastWakeUptime = clock();
    }
}

void ProcessPriorityPolicy::intoBackground(pid_t pid) {
    PidPriorityInfo* pnode = get(pid);
    if (pnode) {
        if (pnode != mTail && pnode != mBackgroundPos) {
            if (mBackgroundPos && mBackgroundPos->last == pnode) {
                return;
            }
            if (pnode->last) pnode->last->next = pnode->next;
            if (pnode->next) {
                pnode->next->last = pnode->last;
            }
            if (mHead == pnode) {
                mHead = pnode->next;
            }
            pnode->next = mBackgroundPos;
            if (mBackgroundPos) {
                pnode->last = mBackgroundPos->last;
                mBackgroundPos->last->next = pnode;
                mBackgroundPos->last = pnode;

            } else {
                pnode->last = mTail;
                mTail->next = pnode;
                mTail = pnode;
            }
            mBackgroundPos = pnode;
        }
    }
}

std::ostream& operator<<(std::ostream& os, ProcessPriorityPolicy& policy) {
    policy.analyseProcessPriority();
    PidPriorityInfo* pnode = policy.mHead;
    os << "\n\nProcess priority OomAdjScore: (pid, score)" << std::endl;
    while (pnode) {
        os << "(" << pnode->pid << "," << pnode->oomScore << ") ";
        pnode = pnode->next;
    }
    os << std::endl;
    return os;
}

} // namespace am
} // namespace os