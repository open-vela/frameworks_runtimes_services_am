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
const static std::string lmkcfg = "/etc/lmk.cfg";
#endif
// The configuration "/data/lmk.cfg" is easy to modify for test
const static std::string lmkcfg2 = "/data/lmk.cfg";

bool LowMemoryManager::init(const std::shared_ptr<os::app::UvLoop>& looper) {
    mLooper = looper;
    memset(mOomScoreThreshold, 0, sizeof(mOomScoreThreshold));

    std::ifstream cfg;
    int cnt = 0;
    cfg.open(lmkcfg);
    if (!cfg.is_open()) {
        ALOGW("LowMemoryManager policy read \"%s\" file", lmkcfg2.c_str());
        cfg.open(lmkcfg2);
    }
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
        cnt = 3;
    }

#ifdef CONFIG_MM_DEFAULT_MANAGER
#ifdef CONFIG_FS_PROCFS_INCLUDE_PRESSURE
    int fd = open("/proc/pressure/memory", O_RDWR);
    if (fd > 0) {
        ALOGW("lmk is reported by poll \"/proc/pressure/memory\"");
        // write maximum oom threshold[cnt-1] and report period".
        dprintf(fd, "%d 2000000", mOomScoreThreshold[cnt - 1][0]);
        auto pollfd = std::make_shared<os::app::UvPoll>(mLooper->get(), fd);
        pollfd->start(
                UV_READABLE | UV_PRIORITIZED,
                [this](int f, int status, int events, void* data) {
                    char buffer[128];
                    const int len = read(f, buffer, 128);
                    if (len > 0) {
                        buffer[len] = 0;
                        int freememory;
                        sscanf(buffer, "remaining %d ", &freememory);
                        executeLMK(freememory);
                    }
                },
                nullptr);
        mPollPressureFds.push_back(pollfd);

    } else
#endif
    {
        ALOGW("lmk is reported by cycle query");
        // Periodicity monitoring:just for test lmk when kernel doesn't provide notifications.
        mTimer.init(mLooper->get(), [this](void*) {
            struct mallinfo info = mallinfo();
            executeLMK(info.fordblks);
        });
        mTimer.start(3000, 3000); // 3 seconds per cycle
    }
#endif

    return true;
} // namespace am

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

void LowMemoryManager::executeLMK(const int freememory) {
    ALOGD("execute low memory kill");
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
                DELAYED_KILLING_TIMEOUT);
        mPidOomScore.erase(pid);
    }
}

} // namespace am
} // namespace os