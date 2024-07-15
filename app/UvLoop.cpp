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

#include "app/Logger.h"

namespace os {
namespace app {

/** Should't use the uv_default_loop() in nuttx if the Memory not isolated */
UvLoop::UvLoop(bool useDefault)
      : mIsDefaultLoop(useDefault),
        mLooper(useDefault ? uv_default_loop() : new uv_loop_t,
                [this](uv_loop_t* loop) { this->destroy(loop); }) {
    if (!mIsDefaultLoop) {
        if (uv_loop_init(mLooper.get()) != 0) {
            ALOGE("UvLoop init failure");
            assert(0);
        }
    }
    mMsgHandler.attachLoop(this->get());
}

UvLoop::UvLoop(uv_loop_t* loop) : mIsDefaultLoop(false), mLooper(loop, [](uv_loop_t*) {}) {
    mMsgHandler.attachLoop(loop);
}

uv_loop_t* UvLoop::get() const {
    return mLooper.get();
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
    return uv_run(mLooper.get(), mode);
}

bool UvLoop::isAlive() {
    return uv_loop_alive(mLooper.get()) != 0;
}

int UvLoop::close() {
    const int ret = uv_loop_close(mLooper.get());
    if (ret) {
        ALOGW("Uvloop close error: loop is busy");
    } else {
        ALOGI("Uvloop close");
    }
    return ret;
}

void UvLoop::stop() {
    mMsgHandler.close();
    uv_stop(mLooper.get());
}

void UvLoop::printAllHandles() {
    FILE* fp = NULL;
#ifdef __NuttX__
    fp = fopen("/dev/log", "wb");
#endif
    fp = fp ? fp : stderr;
    uv_print_all_handles(mLooper.get(), fp);
    if (fp != stderr) {
        fclose(fp);
    }
}

} // namespace app
} // namespace os