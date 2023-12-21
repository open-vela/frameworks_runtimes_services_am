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
#define LOG_TAG "AMS"

#include "LowMemoryManager.h"

#include <signal.h>
#include <string.h>
#include <utils/Log.h>

#include <fstream>
#include <string>

namespace os {
namespace am {

const int DELAYED_KILLING_TIMEOUT = 2000;
#ifdef CONFIG_AM_LMK_CFG
const std::string lmkcfg = CONFIG_AM_LMK_CFG;
#else
const std::string lmkcfg = "/etc/lmk.cfg";
#endif

bool LowMemoryManager::init(const std::shared_ptr<os::app::UvLoop>& looper) {
    mLooper = looper;
    memset(mOomScoreThreshold, 0, sizeof(mOomScoreThreshold));

    std::ifstream cfg(lmkcfg);
    int cnt = 0;
    if (cfg.is_open()) {
        std::string line;
        while (std::getline(cfg, line)) {
            int freeMemory, oomScore;
            if (2 == sscanf(line.c_str(), "%d %d", &freeMemory, &oomScore)) {
                mOomScoreThreshold[cnt][0] = freeMemory;
                mOomScoreThreshold[cnt][1] = oomScore;
                if (++cnt >= MAX_ADJUST_NUM) {
                    break;
                }
            }
        }
    }

    if (cnt == 0) {
        struct mallinfo info = mallinfo();
        ALOGI("system total memory:%d, used:%d free:%d", info.arena, info.uordblks, info.fordblks);
        // if "/etc/lmk.cfg" no configuration data, the lmk warning thresholds are set to 40%, 20%,
        // 10% of system memory.
        int memorylevel[3] = {1, 2, 4};
        int scorethreshold[3] = {100, 500, 700};
        for (unsigned int i = 0; i < sizeof(memorylevel) / sizeof(int); i++) {
            mOomScoreThreshold[i][0] = info.arena * memorylevel[i] / 10;
            mOomScoreThreshold[i][1] = scorethreshold[i];
        }
    }

#ifdef CONFIG_MM_DEFAULT_MANAGER
    // Periodicity monitoring:just for test lmk when kernel doesn't provide notifications.
    mTimer.init(mLooper->get(), [this](void*) {
        struct mallinfo info = mallinfo();
        executeLMK(info);
    });
    mTimer.start(3000, 3000); // 3 seconds per cycle
#endif

    return true;
}

int LowMemoryManager::setPidOomScore(pid_t pid, int score) {
    auto iter = mPidOomScore.find(pid);
    if (iter != mPidOomScore.end()) {
        iter->second = score;
    } else {
        mPidOomScore.emplace(pid, score);
    }
    return 0;
}

int LowMemoryManager::cancelMonitorPid(pid_t pid) {
    auto iter = mPidOomScore.find(pid);
    if (iter != mPidOomScore.end()) {
        mPidOomScore.erase(iter);
    }
    return 0;
}

void LowMemoryManager::setPrepareLMKCallback(const PrepareLMKCB& callback) {
    mPrepareCallback = callback;
}

void LowMemoryManager::setLMKExecutor(const LMKExectorCB& lmkExectorFunc) {
    mExectorCallback = lmkExectorFunc;
}

void LowMemoryManager::executeLMK(struct mallinfo& memoryInfo) {
    ALOGD("execute low memory kill");
    const int freememory = memoryInfo.fordblks;
    std::vector<pid_t> killPidVec;
    for (int i = 0; i < MAX_ADJUST_NUM; i++) {
        if (freememory <= mOomScoreThreshold[i][0]) {
            ALOGI("trigger lmk, mm:%d score:%d", mOomScoreThreshold[i][0],
                  mOomScoreThreshold[i][1]);
            if (mPrepareCallback) mPrepareCallback();
            for (auto iter = mPidOomScore.begin(); iter != mPidOomScore.end(); ++iter) {
                if (iter->second >= mOomScoreThreshold[i][1]) {
                    killPidVec.push_back(iter->first);
                }
            }
            break;
        }
    }

    for (auto pid : killPidVec) {
        if (mExectorCallback) {
            mExectorCallback(pid);
        }
        // Check if the process is finished after a delay time.
        mLooper->postDelayTask(
                [this, pid](void*) {
                    if (mPidOomScore.find(pid) != mPidOomScore.end()) {
                        kill(pid, SIGTERM);
                    }
                },
                DELAYED_KILLING_TIMEOUT, NULL);
        mPidOomScore.erase(pid);
    }
}

} // namespace am
} // namespace os