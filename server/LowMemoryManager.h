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

#include <malloc.h>
#include <sys/types.h>

#include <functional>
#include <list>
#include <unordered_map>

#include "app/UvLoop.h"

namespace os {
namespace am {

class LowMemoryManager {
public:
    using PrepareLMKCB = std::function<void()>;
    using LMKExectorCB = std::function<void(pid_t)>;
    LowMemoryManager() = default;

    bool init(const std::shared_ptr<os::app::UvLoop>& looper);
    void setPrepareLMKCallback(const PrepareLMKCB& callback);
    void setLMKExecutor(const LMKExectorCB& lmkExectorFunc);

    int setPidOomScore(pid_t pid, int score);
    int cancelMonitorPid(pid_t pid);
    void executeLMK(const int freememory);

private:
    const static int MAX_ADJUST_NUM = 5;
    std::shared_ptr<os::app::UvLoop> mLooper;
    std::unordered_map<pid_t, int> mPidOomScore;
    PrepareLMKCB mPrepareCallback;
    LMKExectorCB mExectorCallback;
    int mOomScoreThreshold[MAX_ADJUST_NUM][2];
    os::app::UvTimer mTimer;
    std::vector<std::shared_ptr<os::app::UvPoll>> mPollPressureFds;
};

} // namespace am
} // namespace os
