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

#include <list>
#include <memory>
#include <stack>
#include <string>

#include "ActivityRecord.h"

namespace os {
namespace am {

class ActivityStack {
public:
    explicit ActivityStack(const std::string& tag) : mTag(tag) {}

    bool operator==(const ActivityStack& a) {
        return mTag == a.getTaskTag();
    }

    const std::string& getTaskTag() const;
    const int getSize() const;

    void pushActivity(const ActivityHandler& activity);
    void popActivity();
    ActivityHandler getTopActivity();
    ActivityHandler getRootActivity();
    ActivityHandler findActivity(const std::string& activityName);
    ActivityHandler findActivity(const sp<IBinder>& token);
    void setForeground(const bool isForeground);

    friend std::ostream& operator<<(std::ostream& os, const ActivityStack& obj);

private:
    std::vector<ActivityHandler> mStack;
    std::string mTag;
};

} // namespace am
} // namespace os