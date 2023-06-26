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

#include <utils/Log.h>

namespace os {
namespace app {

/** Should't use the uv_default_loop() in nuttx if the Memory not isolated */
UvLoop::UvLoop(bool useDefault)
      : mIsDefaultLoop(useDefault),
        mLooper(useDefault ? uv_default_loop() : new uv_loop_t,
                [this](uv_loop_t* loop) { this->destroy(loop); }) {
    if (!mIsDefaultLoop) {
        LOG_ALWAYS_FATAL_IF(uv_loop_init(mLooper.get()) != 0, "UvLoop init failure");
    }
}

uv_loop_t* UvLoop::get() const {
    return mLooper.get();
}

int UvLoop::postTask(const UV_CALLBACK& cb, void* data) {
    auto task = new UvAsync(*this, cb);
    return task->sendOnce(data);
}

int UvLoop::postDelayTask(const UV_CALLBACK& cb, uint64_t timeout, void* data) {
    auto task = new UvTimer(*this, [cb, data](void* timer) {
        auto uvTimer = reinterpret_cast<UvTimer*>(timer);
        cb(data);
        uvTimer->stop();
        delete uvTimer;
    });
    return task->start(timeout, 0, this);
}

int UvLoop::run() {
    return uv_run(mLooper.get(), UV_RUN_DEFAULT);
}

bool UvLoop::isAlive() {
    return uv_loop_alive(mLooper.get()) != 0;
}

int UvLoop::close() {
    return uv_loop_close(mLooper.get());
}

void UvLoop::stop() {
    uv_stop(mLooper.get());
}

} // namespace app
} // namespace os