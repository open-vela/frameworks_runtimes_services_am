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

#include "app/Context.h"

namespace os {
namespace app {

class Activity : public ContextWrapper {
public:
    Activity() = default;
    virtual ~Activity() = default;

    virtual void onCreate() = 0;
    virtual void onStart() = 0;
    virtual void onResume() = 0;
    virtual void onPause() = 0;
    virtual void onStop() = 0;
    virtual void onDestory() = 0;

    virtual void onRestart(){};
    virtual void onNewIntent(){};
    virtual void onActivityResult(){};

private:
};

} // namespace app
} // namespace os