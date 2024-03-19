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

#include <string_view>
#include <vector>

#include "app/ActivityManager.h"

namespace os {
namespace app {

class AmCommand {
public:
    AmCommand();
    ~AmCommand();

    int showUsage();
    int run(int argc, char* argv[]);

private:
    std::string_view nextArg();
    int makeIntent(Intent& intent);

    int startActivity();
    int stopActivity();
    int startService();
    int stopService();
    int postIntent();
    int dump();

private:
    ActivityManager mAm;
    std::vector<std::string_view> mArgs;
    size_t mNextArgs;
};

} // namespace app
} // namespace os