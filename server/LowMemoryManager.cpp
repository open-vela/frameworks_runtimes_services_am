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

#include "LowMemoryManager.h"

#include <signal.h>
#include <string.h>

#include <fstream>
#include <string>

#include "app/Logger.h"

namespace os {
namespace am {

const int DELAYED_KILLING_TIMEOUT = 12000;
const float MIN_MEM_THRESH = 0.022; // Used to calculate the minimum memory available to the system

#ifdef CONFIG_AM_LMK_CFG
const std::string lmkcfg = CONFIG_AM_LMK_CFG;
#else
const static std::string lmkcfg = "/etc/lmk.cfg";
#endif
// The configuration "/data/lmk.cfg" is easy to modify for test
const static std::string lmkcfg_debug = "/data/lmk.cfg";

bool LowMemoryManager::init(const std::shared_ptr<os::app::UvLoop>& looper) {
    mLooper = looper;
    memset(mOomScoreThreshold, 0, sizeof(mOomScoreThreshold));

    std::ifstream cfg;
    int cnt = 0;
    cfg.open(lmkcfg_debug);
    if (!cfg.is_open()) {
        ALOGW("LowMemoryManager policy read \"%s\" file", lmkcfg.c_str());
        cfg.open(lmkcfg);
    }
    if (cfg.is_open()) {
        std::string line;
        while (std::getline(cfg, line)) {
            int freeMemory, oomScore, maxBlock;
            if (3 == sscanf(line.c_str(), "%d %d %d", &freeMemory, &maxBlock, &oomScore)) {
                mOomScoreThreshold[cnt][0] = freeMemory;
                mOomScoreThreshold[cnt][1] = maxBlock;
                mOomScoreThreshold[cnt][2] = oomScore;
                if (++cnt >= MAX_ADJUST_NUM) {
                    break;
                }
            }
        }
    }

    struct mallinfo info = mallinfo();
    mMinMemoryThreshold = info.arena * MIN_MEM_THRESH;

    if (cnt == 0) {
        ALOGI("system total memory:%d, used:%d free:%d", info.arena, info.uordblks, info.fordblks);
        // if "/etc/lmk.cfg" no configuration data, the lmk warning thresholds are set to 40%, 20%,
        // 10% of system memory.
        int memorylevel[3] = {1, 2, 4};
        int scorethreshold[3] = {10, 102, 500};
        for (unsigned int i = 0; i < sizeof(memorylevel) / sizeof(int); i++) {
            mOomScoreThreshold[i][0] = info.arena * memorylevel[i] / 10;
            mOomScoreThreshold[i][1] = mOomScoreThreshold[i][0] - 1024 * 1024 * 2;
            mOomScoreThreshold[i][2] = scorethreshold[i];
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
                        ALOGD("poll pressure:%s", buffer);
                        int freememory, maxblock;
                        if (2 ==
                            sscanf(buffer, "remaining %d, largest:%d", &freememory, &maxblock)) {
                            executeLMK(freememory, maxblock);
                        }
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
            struct mallinfo meminfo = mallinfo();
            executeLMK(meminfo.fordblks, meminfo.mxordblk);
        });
        mTimer.start(2000, 2000); // 2 seconds per cycle
    }
#endif
    return true;
} // namespace am

bool LowMemoryManager::isOkToLaunch() {
    // check memory
    unsigned int maxblock = -1;
#ifdef CONFIG_FS_PROCFS_INCLUDE_PRESSURE
    int fd = open("/proc/pressure/memory", O_RDONLY);
    if (fd > 0) {
        char buf[256];
        const int len = read(fd, buf, 256);
        if (len > 0) {
            buf[len] = 0;
            int free;
            if (sscanf(buf, "remaining %d, largest:%d", &free, &maxblock) == 2) {
                ALOGW("pressure format error:%s", buf);
            }
        }
        close(fd);
    }
#else
    struct mallinfo info = mallinfo();
    maxblock = info.mxordblk;
#endif
    if (maxblock < mMinMemoryThreshold) {
        ALOGW("system memory less then min use threshold! current:%d, threshold:%d", maxblock,
              mMinMemoryThreshold);
        return false;
    }

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

void LowMemoryManager::executeLMK(const int freememory, const int maxblock) {
    ALOGD("execute low memory kill");
    std::vector<pid_t> killPidVec;
    for (int i = 0; i < MAX_ADJUST_NUM; i++) {
        if (freememory <= mOomScoreThreshold[i][0] || maxblock <= mOomScoreThreshold[i][1]) {
            if (mPrepareCallback) mPrepareCallback();
            for (auto iter = mPidOomScore.begin(); iter != mPidOomScore.end(); ++iter) {
                if (iter->second >= mOomScoreThreshold[i][2]) {
                    ALOGI("LMK free:%d maxblock:%d score:%d, kill pid:%d score:%d", freememory,
                          maxblock, mOomScoreThreshold[i][2], iter->first, iter->second);
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
        // mLooper->postDelayTask(
        //         [this, pid](void*) {
        //             // kill -0, check if process is running
        //             if (kill(pid, 0) == 0) {
        //                 ALOGW("warning, kill the app:%d by signal!!!!", pid);
        //                 kill(pid, SIGTERM);
        //             }
        //         },
        //         DELAYED_KILLING_TIMEOUT);
        mPidOomScore.erase(pid);
    }
}

} // namespace am
} // namespace os