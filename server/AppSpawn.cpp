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

#define LOG_TAG "AppSpawn"

#include "AppSpawn.h"

#include <nuttx/config.h>
#include <pthread.h>
#include <spawn.h>
#include <string.h>
#include <sys/wait.h>
#include <utils/Log.h>

namespace os {
namespace app {

ChildPidExitCB AppSpawn::gChildPidExitCB;

static void childHandler(int signum) {
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
            // exit normal
            ALOGW("child process:%d normal exit:%d", pid, WEXITSTATUS(status));
            AppSpawn::gChildPidExitCB((int)pid);

        } else if (WIFSIGNALED(status)) {
            ALOGE("child process:%d exception exit by signal:%d", pid, WTERMSIG(status));
            AppSpawn::gChildPidExitCB((int)pid);
        }
    }
}

int AppSpawn::signalInit(const ChildPidExitCB& cb) {
    gChildPidExitCB = cb;
    struct sigaction sig_chld;
    sig_chld.sa_flags = SA_SIGINFO;
    sig_chld.sa_handler = childHandler;
    if (sigaction(SIGCHLD, &sig_chld, nullptr) < 0) {
        ALOGE("setting SIGCHLD handler error:%s", strerror(errno));
    }

    struct sigaction sig_hup = {};
    sig_hup.sa_handler = SIG_IGN;
    if (sigaction(SIGHUP, &sig_hup, nullptr) < 0) {
        ALOGE("setting SIGHUP handler:%s error", strerror(errno));
    }
    return 0;
}

int AppSpawn::appSpawn(const char* execfile, std::initializer_list<std::string> argvlist) {
    int pid = -1;
    char* argv[argvlist.size() + 2]; /** 2 = program name + null ptr */
    int i = 1;
    argv[0] = const_cast<char*>(execfile);
    for (auto it = argvlist.begin(); it != argvlist.end(); ++it) {
        argv[i++] = const_cast<char*>(it->c_str());
    }
    argv[i] = nullptr;

    const int ret = posix_spawn(&pid, execfile, NULL, NULL, argv, NULL);
    if (ret < 0) {
        ALOGE("posix_spawn %s failed error:%d", execfile, ret);
        return ret;
    }
    return pid;
}

} // namespace app
} // namespace os