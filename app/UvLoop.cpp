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

#include "app/UvLoop.h"

#include <cassert>

#include "app/Logger.h"

namespace os {
namespace app {

/** Should't use the uv_default_loop() in nuttx if the Memory not isolated */
UvLoop::UvLoop(bool useDefault) : mIsDefaultLoop(useDefault), mLooper(new uv_loop_t) {
    if (mIsDefaultLoop) {
        if (uv_loop_init(mLooper) != 0) {
            ALOGE("UvLoop init failure");
            assert(0);
        }
    }
    mMsgHandler.attachLoop(mLooper);
}

UvLoop::UvLoop(uv_loop_t* loop) : mIsDefaultLoop(false), mLooper(loop) {
    mMsgHandler.attachLoop(loop);
}

UvLoop::~UvLoop() {
    mMsgHandler.close();
    if (mIsDefaultLoop) {
        destroy(mLooper);
        mLooper = nullptr;
    }
}

uv_loop_t* UvLoop::get() const {
    return mLooper;
}

int UvLoop::postDelayTask(const UV_CALLBACK& cb, uint64_t timeout, void* data) {
    auto func = [](const UV_CALLBACK& callback, void* d, void* timer) {
        auto uvTimer = reinterpret_cast<UvTimer*>(timer);
        callback(d);
        uvTimer->stop();
        delete uvTimer;
    };
    auto bindFunc = std::bind(func, cb, data, std::placeholders::_1);
    auto timerTask = new UvTimer();
    timerTask->init(get(), bindFunc);
    return timerTask->start(timeout, 0, timerTask);
}

int UvLoop::run(uv_run_mode mode) {
    return uv_run(mLooper, mode);
}

bool UvLoop::isAlive() {
    return uv_loop_alive(mLooper) != 0;
}

void UvLoop::destroy(uv_loop_t* loop) const {
    if (loop) {
        uv_loop_close(loop);
        delete loop;
    }
}

int UvLoop::close() {
    const int ret = uv_loop_close(mLooper);
    if (ret) {
        ALOGW("Uvloop close error: loop is busy");
    } else {
        ALOGI("Uvloop close");
    }
    return ret;
}

void UvLoop::stop() {
    mMsgHandler.close();
    uv_stop(mLooper);
}

void UvLoop::printAllHandles() {
    FILE* fp = NULL;
#ifdef __NuttX__
    fp = fopen("/dev/log", "wb");
#endif
    fp = fp ? fp : stderr;
    uv_print_all_handles(mLooper, fp);
    if (fp != stderr) {
        fclose(fp);
    }
}

} // namespace app
} // namespace os
