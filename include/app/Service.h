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

#include <string>

#include "app/Context.h"

namespace os {
namespace app {

class Service : public ContextWrapper {
public:
    Service() = default;
    virtual ~Service() = default;
    /** The status is part of the ServiceRecord inside */
    enum {
        CREATED = 1,
        STARTED = 3,
        DESTROYED = 5,
    };

    virtual void onCreate() = 0;
    virtual void onStartCommand(const Intent& intent) = 0;
    virtual void onDestory() = 0;

    void setServiceName(const string& name);
    const std::string& getServiceName();
    void reportServiceStatus(int status);

private:
    std::string mServiceName;
};

} // namespace app
} // namespace os