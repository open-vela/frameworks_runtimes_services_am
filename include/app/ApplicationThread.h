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

#include <queue>

#include "app/Application.h"
#include "app/UvLoop.h"

namespace os {
namespace app {

/**
 * @brief A type alias for a function callback that takes a `void*` parameter.
 */
using TASK_CALLBACK = std::function<void(void*)>;

class ApplicationThreadStub;

/**
 * @class ApplicationThread
 * @brief Represents a thread responsible for running the application's event loop.
 *
 * The `ApplicationThread` class extends `UvLoop` and is responsible for executing
 * the main event loop of the application. It interacts with the `Application` class
 * and provides a mechanism for managing application tasks in a separate thread.
 */
class ApplicationThread {
public:
    /**
     * @brief Constructor for the ApplicationThread class.
     *
     * @param[in] app The application associated with this thread.
     */
    ApplicationThread(Application* app);
    /**
     * @brief Destructor for the ApplicationThread class.
     */
    ~ApplicationThread();
    /**
     * @brief Runs the main loop for the ApplicationThread.
     *
     * This function is responsible for starting the event loop and processing tasks.
     *
     * @param[in] argc The number of command line arguments.
     * @param[in] argv The array of command line arguments.
     * @return An integer status code indicating the result of the main loop execution.
     */
    int start(int argc, char** argv);
    /**
     * @brief Stop the application thread.
     *
     * Stops the event loop and halts the execution of the thread.
     */
    void stop();

private:
    UvLoop mLoop; /**< A pointer to the event loop associated with this thread. */
    android::sp<ApplicationThreadStub>
            mAppThreadStub; /**< A pointer to the ApplicationThreadStub instance. */
    Application* mApp;      /**< A pointer to the Application instance managed by this thread. */
};

} // namespace app
} // namespace os