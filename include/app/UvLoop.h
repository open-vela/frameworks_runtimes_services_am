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

#include <uv.h>

#include <functional>
#include <memory>
#include <mutex>
#include <queue>

namespace os {
namespace app {

using UV_CALLBACK = std::function<void(void*)>;

inline void uvCloseHandle(uv_handle_t* handler) {
    if (!uv_is_closing(handler)) {
        uv_close(handler, NULL);
    }
}

template <typename T>
class UvMsgQueue {
public:
    UvMsgQueue() {}
    virtual ~UvMsgQueue() {}

    int attachLoop(uv_loop_t* loop) {
        mUvAsync.data = this;
        return uv_async_init(loop, &mUvAsync, [](uv_async_t* handle) {
            UvMsgQueue* my = reinterpret_cast<UvMsgQueue*>(handle->data);
            my->processMessage();
        });
    }

    int push(T& msg) {
        std::lock_guard<std::mutex> lock(mMutex);
        mQueue.push(msg);
        return uv_async_send(&mUvAsync);
    }

    template <class... Args>
    int emplace(Args&&... args) {
        std::lock_guard<std::mutex> lock(mMutex);
        mQueue.emplace(std::forward<Args>(args)...);
        return uv_async_send(&mUvAsync);
    }

    virtual void handleMessage(const T& msg) = 0;

    void close() {
        uvCloseHandle((uv_handle_t*)&mUvAsync);
    }

private:
    void processMessage() {
        std::lock_guard<std::mutex> lock(mMutex);
        while (!mQueue.empty()) {
            handleMessage(mQueue.front());
            mQueue.pop();
        }
    }

private:
    std::mutex mMutex;
    std::queue<T> mQueue;
    uv_async_t mUvAsync;
};

class UvLoop {
public:
    UvLoop(bool useDefault = false);
    /** use exist uvloop */
    UvLoop(uv_loop_t* loop);

    using TaskCB = std::function<void()>;
    struct MsgCB {
        TaskCB callback;
        MsgCB(const TaskCB& cb) : callback(cb) {}
    };
    class MessageHandler : public UvMsgQueue<MsgCB> {
        void handleMessage(const MsgCB& msg) override {
            msg.callback();
        }
    };
    int postTask(TaskCB&& cb) {
        return mMsgHandler.emplace(cb);
    }

    uv_loop_t* get() const;
    int postDelayTask(const UV_CALLBACK& callback, uint64_t timeout, void* data = nullptr);

    int run(uv_run_mode mode = UV_RUN_DEFAULT);
    bool isAlive();
    int close();
    void stop();
    void printAllHandles();

private:
    // Custom deleter
    typedef std::function<void(uv_loop_t*)> Deleter;
    void destroy(uv_loop_t* loop) const {
        if (!mIsDefaultLoop) {
            delete loop;
        }
    }
    bool mIsDefaultLoop;
    std::unique_ptr<uv_loop_t, Deleter> mLooper;
    MessageHandler mMsgHandler;
};

class UvAsync {
public:
    UvAsync() {}
    UvAsync(uv_loop_t* loop, const UV_CALLBACK& cb) {
        init(loop, cb);
    }
    ~UvAsync() {
        close();
    }

    int init(uv_loop_t* loop, const UV_CALLBACK& cb) {
        mWillDelete = false;
        mCallback = cb;
        mHandler.data = this;
        return uv_async_init(loop, &mHandler, [](uv_async_t* handle) {
            UvAsync* my = reinterpret_cast<UvAsync*>(handle->data);
            my->mCallback(my->mData);
            if (my->mWillDelete) {
                uv_close((uv_handle_t*)handle, [](uv_handle_t* asynct) {
                    delete reinterpret_cast<UvAsync*>(asynct->data);
                });
            }
        });
    }

    /** send and delete self after be processed */
    int sendOnce(void* data = nullptr) {
        mWillDelete = true;
        return send(data);
    }

    int send(void* data = nullptr) {
        mData = data;
        return uv_async_send(&mHandler);
    }

    void close() {
        uvCloseHandle((uv_handle_t*)&mHandler);
    }

private:
    uv_async_t mHandler;
    bool mWillDelete;
    UV_CALLBACK mCallback;
    void* mData;
};

class UvTimer {
public:
    UvTimer() {
        mHandler = new uv_timer_t;
    }
    UvTimer(uv_loop_t* loop, const UV_CALLBACK& cb) {
        mHandler = new uv_timer_t;
        init(loop, cb);
    }
    ~UvTimer() {
        close();
    }

    int init(uv_loop_t* loop, const UV_CALLBACK& cb) {
        mCallback = cb;
        mHandler->data = this;
        return uv_timer_init(loop, mHandler);
    }

    int start(int64_t timeout, int64_t repeat = 0, void* data = nullptr) {
        mData = data;
        return uv_timer_start(
                mHandler,
                [](uv_timer_t* handle) {
                    UvTimer* my = reinterpret_cast<UvTimer*>(handle->data);
                    my->mCallback(my->mData);
                },
                timeout, repeat);
    }

    int stop() {
        return uv_timer_stop(mHandler);
    }

    int again() {
        return uv_timer_again(mHandler);
    }

    void close() {
        if (mHandler && !uv_is_closing((uv_handle_t*)mHandler)) {
            uv_close((uv_handle_t*)mHandler,
                     [](uv_handle_t* handler) { delete reinterpret_cast<uv_timer_t*>(handler); });
            mHandler = nullptr;
        }
    }

private:
    uv_timer_t* mHandler;
    UV_CALLBACK mCallback;
    void* mData;
};

class UvPoll {
public:
    UvPoll() {}
    UvPoll(uv_loop_t* loop, int fd) {
        init(loop, fd);
    }
    ~UvPoll() {
        close();
    }

    int init(uv_loop_t* loop, int fd) {
        mHandler.data = this;
        return uv_poll_init(loop, &mHandler, fd);
    }

    using PollCallBack = std::function<void(int fd, int status, int events, void* data)>;
    int start(int event, const PollCallBack& cb, void* data = nullptr) {
        mCallback = cb;
        mData = data;
        return uv_poll_start(&mHandler, event, [](uv_poll_t* handle, int status, int events) {
            UvPoll* my = reinterpret_cast<UvPoll*>(handle->data);
            my->mCallback(handle->io_watcher.fd, status, events, my->mData);
        });
    }

    int stop() {
        return uv_poll_stop(&mHandler);
    }

    void close() {
        uvCloseHandle((uv_handle_t*)&mHandler);
    }

private:
    uv_poll_t mHandler;
    PollCallBack mCallback;
    void* mData;
};

} // namespace app
} // namespace os