/*
 * Copyright (C) 2025 Xiaomi Corporation
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

#include <nuttx/config.h>
#include <sys/types.h>
#include <uv.h>

#ifdef CONFIG_SYSTEM_SERVER_LITE
typedef int AppId;
#else
typedef pid_t AppId;
#endif

namespace os {
namespace app {

AppId getXmsAppId();

uv_loop_t* getXmsLoop();

void setXmsLoop(uv_loop_t* loop);

AppId getNextXmsAppId();

} // namespace app
} // namespace os