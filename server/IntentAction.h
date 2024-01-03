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

#include <map>
#include <string>
#include <vector>

namespace os {
namespace am {

using std::map;
using std::string;
using std::vector;

#define COMPONENT_NAME_SPLICE(p, c) (p + '/' + c)

class IntentAction {
public:
    enum ComponentType {
        COMP_TYPE_ACTIVITY,
        COMP_TYPE_SERVICE,
    };
    bool getSingleTargetByAction(const string& action, string& target, const ComponentType type);
    bool getMultiTargetByAction(const string& action, vector<string>& targetlist,
                                const ComponentType type);
};

} // namespace am
} // namespace os