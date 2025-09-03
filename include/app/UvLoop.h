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

/**
 * @brief Type alias for a callback function that takes a `void*` as an argument.
 */
using UV_CALLBACK = std::function<void(void*)>;

/**
 * @brief Closes a libuv handle if it is not already in the process of closing.
 *
 * @param[in] handler The uv handle to be closed.
 */
inline void uvCloseHandle(uv_handle_t* handler) {
    if (!uv_is_closing(handler)) {
        uv_close(handler, NULL);
    }
}

/**
 * @class UvMsgQueue
 * @brief A message queue that posts messages to a UV loop asynchronously.
 *
 * `UvMsgQueue` is a thread-safe queue that posts messages to a UV loop for asynchronous processing.
 * The queue uses an `uv_async_t` handle to notify the loop when a new message is available to
 * process.
 *
 * @tparam T The type of messages to be handled by the queue.
 */
template <typename T>
class UvMsgQueue {
public:
    /**
     * @brief Default constructor for the UvMsgQueue.
     */
    UvMsgQueue() {
        mUvAsync = new uv_async_t;
    }
    /**
     * @brief Default destructor for the UvMsgQueue.
     */
    virtual ~UvMsgQueue() {}
    /**
     * @brief Attaches the message queue to a libuv loop.
     *
     * This function initializes the `uv_async_t` handle and associates it with the provided
     * `uv_loop_t`. When a message is pushed into the queue, the libuv event loop will invoke the
     * callback to process the message.
     *
     * @param loop The libuv loop to attach to.
     * @return Returns `0` on success, or a non-zero value on error.
     */
    int attachLoop(uv_loop_t* loop) {
        mUvAsync->data = this;
        return uv_async_init(loop, mUvAsync, [](uv_async_t* handle) {
            UvMsgQueue* my = reinterpret_cast<UvMsgQueue*>(handle->data);
            my->processMessage();
        });
    }
    /**
     * @brief Pushes a message to the queue and triggers processing.
     *
     * This function adds a message to the queue and signals the event loop to process the message
     * asynchronously.
     *
     * @param[in] msg The message to be pushed.
     * @return Returns 0 on success, or a non-zero value on error.
     */
    int push(T& msg) {
        return emplace(std::move(msg));
    }
    /**
     * @brief Emplaces a message to the queue and triggers processing.
     *
     * This function emplaces a message into the queue (using perfect forwarding) and triggers the
     * event loop to process it.
     *
     * @tparam[in] Args The argument types used to construct the message.
     * @param[in] args Arguments to construct the message.
     * @return Returns 0 on success, or a non-zero value on error.
     */
    template <class... Args>
    int emplace(Args&&... args) {
        int cnt{0};
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mQueue.emplace(std::forward<Args>(args)...);
            cnt = mQueue.size();
        }
        if (cnt == 1) {
            // If this is the first message, we need to trigger processing
            return uv_async_send(mUvAsync);
        }

        return 0;
    }
    /**
     * @brief Handle messages from the queue.
     *
     * @param[in] msg The message to be processed.
     */
    virtual void handleMessage(const T& msg) = 0;
    /**
     * @brief Closes the message queue and cleans up resources.
     */
    void close() {
        if (mUvAsync && !uv_is_closing((uv_handle_t*)mUvAsync)) {
            uv_close((uv_handle_t*)mUvAsync,
                     [](uv_handle_t* handler) { delete reinterpret_cast<uv_async_t*>(handler); });
            mUvAsync = nullptr;
        }
    }

private:
    /**
     * @brief Processes and handles all messages in the queue.
     */
    void processMessage() {
        std::queue<T> tempQueue;
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (mQueue.empty()) {
                return; // No messages to process
            }
            // Swap the queue to avoid holding the lock while processing
            tempQueue.swap(mQueue);
        }
        while (!tempQueue.empty()) {
            handleMessage(tempQueue.front());
            tempQueue.pop();
        }
    }

private:
    std::mutex mMutex;    /**< Mutex to protect access to the queue. */
    std::queue<T> mQueue; /**< The queue of messages to be processed. */
    uv_async_t* mUvAsync; /**< The uv_async handle for triggering message processing. */
};

/**
 * @class UvLoop
 * @brief Represents the event loop for the application.
 */
class UvLoop {
public:
    /**
     * @brief Constructs a UvLoop using the default loop.
     *
     * @param[in] useDefault Whether to use the default loop.
     */
    UvLoop(bool useDefault = true);
    /**
     * @brief Constructs a UvLoop using an existing libuv loop.
     *
     * @param[in] loop An existing `uv_loop_t` to use.
     */
    UvLoop(uv_loop_t* loop);
    /**
     * @brief Default destructor for the UvLoop class.
     */
    ~UvLoop();
    /**
     * @brief A callback type for tasks posted to the loop.
     */
    using TaskCB = std::function<void()>;
    /**
     * @brief A structure used for message callbacks in the event loop.
     */
    struct MsgCB {
        TaskCB callback;
        MsgCB(const TaskCB& cb) : callback(cb) {}
    };
    /**
     * @class MessageHandler
     * @brief A message handler class that processes task messages.
     *
     * This class is used internally to process tasks in the event loop.
     */
    class MessageHandler : public UvMsgQueue<MsgCB> {
        /**
         * @brief Handle messages from the queue.
         *
         * @param[in] msg The message to be processed.
         */
        void handleMessage(const MsgCB& msg) override {
            msg.callback();
        }
    };
    /**
     * @brief Posts a task to the event loop for execution.
     *
     * This function adds a task (callback) to be executed in the event loop.
     *
     * @param[in] cb The callback function to be executed.
     * @return Returns 0 on success, or a non-zero value on error.
     */
    int postTask(TaskCB&& cb) {
        return mMsgHandler.emplace(cb);
    }
    /**
     * @brief Retrieves the underlying uv_loop_t handle.
     *
     * @return The `uv_loop_t` handle.
     */
    uv_loop_t* get() const;
    /**
     * @brief Posts a delayed task to the event loop.
     *
     * This function posts a task to the event loop that will be executed after a specified delay.
     *
     * @param[in] callback The callback function to be executed.
     * @param[in] timeout The delay (in milliseconds) before the task is executed.
     * @param[in] data Optional data to pass to the callback.
     * @return Returns 0 on success, or a non-zero value on error.
     */
    int postDelayTask(const UV_CALLBACK& callback, uint64_t timeout, void* data = nullptr);
    /**
     * @brief Runs the event loop.
     *
     * This function starts the event loop and keeps it running until the loop is stopped.
     *
     * @param[in] mode The run mode.
     * @return Returns 0 on success, or a non-zero value on error.
     */
    int run(uv_run_mode mode = UV_RUN_DEFAULT);
    /**
     * @brief Checks if the event loop is alive (still running).
     *
     * @return True if the event loop is running, false otherwise.
     */
    bool isAlive();
    /**
     * @brief Closes the event loop and frees resources.
     *
     * This function stops the event loop and releases any resources associated with it.
     *
     * @return Returns 0 on success, or a non-zero value on error.
     */
    int close();
    /**
     * @brief Stops the event loop immediately.
     *
     * This function stops the event loop from running and schedules it for closing.
     */
    void stop();
    /**
     * @brief Prints information about all active handles in the event loop.
     */
    void printAllHandles();

private:
    /**
     * @brief Custom deleter for `uv_loop_t` to manage the event loop lifecycle.
     */
    typedef std::function<void(uv_loop_t*)> Deleter;
    /**
     * @brief Destroy a libuv loop.
     *
     * @param[in] loop The `uv_loop_t` instance to be destroyed.
     */
    void destroy(uv_loop_t* loop) const;

    bool mIsDefaultLoop;        /**< Whether the loop is the default libuv loop. */
    uv_loop_t* mLooper;         /**< The `uv_loop_t` object managed by the loop. */
    MessageHandler mMsgHandler; /**< The message handler used for processing tasks. */
};

/**
 * @class UvAsync
 * @brief A class for handling asynchronous events with libuv.
 *
 * This class wraps the `uv_async_t` handle to provide an easy-to-use interface for posting tasks to
 * the event loop. The task is processed asynchronously when triggered by `uv_async_send()`.
 */
class UvAsync {
public:
    /**
     * @brief Default constructor for the UvAsync class.
     */
    UvAsync() {}
    /**
     * @brief Constructor that initializes the async handle with a loop and a callback.
     *
     * @param[in] loop The `uv_loop_t` to associate with the async handle.
     * @param[in] cb The callback function to be executed when the task is triggered.
     */
    UvAsync(uv_loop_t* loop, const UV_CALLBACK& cb) {
        init(loop, cb);
    }
    /**
     * @brief Destructor for the UvAsync class.
     */
    ~UvAsync() {
        close();
    }
    /**
     * @brief Initializes the async handle with a loop and a callback.
     *
     * @param[in] loop The `uv_loop_t` to associate with the async handle.
     * @param[in] cb The callback function to be executed when the async handle is triggered.
     * @return Returns 0 on success, or a non-zero value on error.
     */
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

    /**
     * @brief Sends an asynchronous message and deletes the object after processing.
     *
     * This function sends the message and ensures that the object is deleted after the message is
     * processed.
     *
     * @param[in] data Optional data to be passed to the callback.
     * @return Returns 0 on success, or a non-zero value on error.
     */
    int sendOnce(void* data = nullptr) {
        mWillDelete = true;
        return send(data);
    }
    /**
     * @brief Sends an asynchronous message.
     *
     * This function sends the message to the async handle and triggers the callback when processed.
     *
     * @param[in] data Optional data to be passed to the callback.
     * @return Returns 0 on success, or a non-zero value on error.
     */
    int send(void* data = nullptr) {
        mData = data;
        return uv_async_send(&mHandler);
    }
    /**
     * @brief Closes the async handle and releases resources.
     */
    void close() {
        uvCloseHandle((uv_handle_t*)&mHandler);
    }

private:
    uv_async_t mHandler;   /**< The libuv async handle. */
    bool mWillDelete;      /**< Whether the object should be deleted after processing. */
    UV_CALLBACK mCallback; /**< The callback to execute when the task is processed. */
    void* mData;           /**< The data passed to the callback. */
};

/**
 * @class UvTimer
 * @brief A class for creating and managing libuv timers.
 *
 * This class provides a wrapper for `uv_timer_t`, allowing you to start, stop, and restart timers
 * with custom callbacks.
 */
class UvTimer {
public:
    /**
     * @brief Default constructor for the UvTimer class.
     *
     * This initializes the timer handle but does not associate it with a loop yet.
     */
    UvTimer() {
        mHandler = new uv_timer_t;
    }
    /**
     * @brief Constructor that initializes the timer with a loop and a callback.
     *
     * @param[in] loop The `uv_loop_t` to associate with the timer.
     * @param[in] cb The callback function to be executed when the timer expires.
     */
    UvTimer(uv_loop_t* loop, const UV_CALLBACK& cb) {
        mHandler = new uv_timer_t;
        init(loop, cb);
    }
    /**
     * @brief Destructor for the UvTimer class.
     *
     * This ensures the timer handle is closed and cleaned up.
     */
    ~UvTimer() {
        close();
    }
    /**
     * @brief Initializes the timer with a loop and a callback.
     *
     * @param[in] loop The `uv_loop_t` to associate with the timer.
     * @param[in] cb The callback function to be executed when the timer expires.
     * @return Returns 0 on success, or a non-zero value on error.
     */
    int init(uv_loop_t* loop, const UV_CALLBACK& cb) {
        mCallback = cb;
        mHandler->data = this;
        return uv_timer_init(loop, mHandler);
    }
    /**
     * @brief Starts the timer with the specified timeout and repeat interval.
     *
     * @param[in] timeout The initial timeout (in milliseconds).
     * @param[in] repeat The repeat interval (in milliseconds, 0 for no repeat).
     * @param[in] data Optional data to pass to the callback.
     * @return Returns `0` on success, or a non-zero value on error.
     */
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
    /**
     * @brief Stops the timer.
     *
     * @return Returns 0 on success, or a non-zero value on error.
     */
    int stop() {
        return uv_timer_stop(mHandler);
    }
    /**
     * @brief Restarts the timer to trigger again after the specified interval.
     *
     * @return Returns 0 on success, or a non-zero value on error.
     */
    int again() {
        return uv_timer_again(mHandler);
    }
    /**
     * @brief Closes the timer and releases resources.
     */
    void close() {
        if (mHandler && !uv_is_closing((uv_handle_t*)mHandler)) {
            uv_close((uv_handle_t*)mHandler,
                     [](uv_handle_t* handler) { delete reinterpret_cast<uv_timer_t*>(handler); });
            mHandler = nullptr;
        }
    }

private:
    uv_timer_t* mHandler;  /**< The libuv timer handle. */
    UV_CALLBACK mCallback; /**< The callback to execute when the timer expires. */
    void* mData;           /**< The data passed to the callback. */
};

/**
 * @class UvPoll
 * @brief A class for handling file descriptor-based events in libuv.
 *
 * This class wraps the `uv_poll_t` handle to allow monitoring file descriptors for events such as
 * readability and writability. It can be used to monitor sockets or any other file descriptors.
 */
class UvPoll {
public:
    /**
     * @brief Default constructor for the UvPoll class.
     */
    UvPoll() {
        mHandler = new uv_poll_t;
    }
    /**
     * @brief Constructor that initializes the poll handle with a loop and a file descriptor.
     *
     * @param[in] loop The `uv_loop_t` to associate with the poll handle.
     * @param[in] fd The file descriptor to monitor.
     */
    UvPoll(uv_loop_t* loop, int fd) {
        mHandler = new uv_poll_t;
        init(loop, fd);
    }
    /**
     * @brief Destructor for the UvPoll class.
     *
     * This ensures the poll handle is closed and cleaned up.
     */
    ~UvPoll() {
        close();
    }
    /**
     * @brief Initializes the poll handle with a loop and a file descriptor.
     *
     * @param[in] loop The `uv_loop_t` to associate with the poll handle.
     * @param[in] fd The file descriptor to monitor.
     * @return Returns 0 on success, or a non-zero value on error.
     */
    int init(uv_loop_t* loop, int fd) {
        mHandler->data = this;
        return uv_poll_init(loop, mHandler, fd);
    }
    /**
     * @brief A callback type for handling poll events.
     */
    using PollCallBack = std::function<void(int fd, int status, int events, void* data)>;
    /**
     * @brief Starts polling the file descriptor for events.
     *
     * @param[in] event The events to monitor (e.g., read, write).
     * @param[in] cb The callback function to be executed when the events occur.
     * @param[in] data Optional data to be passed to the callback function.
     * @return Returns 0 on success, or a non-zero value on error.
     */
    int start(int event, const PollCallBack& cb, void* data = nullptr) {
        mCallback = cb;
        mData = data;
        return uv_poll_start(mHandler, event, [](uv_poll_t* handle, int status, int events) {
            UvPoll* my = reinterpret_cast<UvPoll*>(handle->data);
            my->mCallback(handle->io_watcher.fd, status, events, my->mData);
        });
    }
    /**
     * @brief Stops monitoring the file descriptor.
     *
     * @return Returns 0 on success, or a non-zero value on error.
     */
    int stop() {
        return uv_poll_stop(mHandler);
    }
    /**
     * @brief Closes the poll handle and releases resources.
     */
    void close() {
        if (mHandler && !uv_is_closing((uv_handle_t*)mHandler)) {
            uv_close((uv_handle_t*)mHandler,
                     [](uv_handle_t* handler) { delete reinterpret_cast<uv_poll_t*>(handler); });
            mHandler = nullptr;
        }
    }

private:
    uv_poll_t* mHandler;    /**< The libuv poll handle */
    PollCallBack mCallback; /**< The callback to invoke when an event occurs */
    void* mData;            /**< Optional data to pass to the callback */
};

} // namespace app
} // namespace os