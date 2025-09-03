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

/**
 * This file is introduced by developer and be used to entry "main()"
 * eg:
 * class MyApplication {
 *     ...you code...
 * };
 * #define APPLICATION MyApplication
 * #include <app/AppMain.h>
 */

#include "XMSConfig.h"

/**
 * @brief The entry point for the application.
 *
 * This function initializes the application and runs the main event loop
 * using the ApplicationThread class. It processes command line arguments
 * and starts the application thread to handle the main loop.
 *
 * @param[in] argc The number of command line arguments.
 * @param[in] argv The array of command line arguments.
 * @return An integer status code, where 0 indicates successful execution.
 */
extern "C" int main(int argc, char** argv) {
    if (xmsLiteMode()) {
        auto* app = new APPLICATION;
        os::app::ApplicationThread* appThread = new os::app::ApplicationThread(app);
        appThread->start(argc, argv);
        return 0;
    }
    APPLICATION app;
    os::app::ApplicationThread appThread(&app);
    appThread.start(argc, argv);
    return 0;
}
