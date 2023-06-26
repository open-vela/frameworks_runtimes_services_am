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

#include "IntentAction.h"

namespace os {
namespace am {

void IntentAction::setIntentAction(const string& action, const string& activityName) {
    auto iter = mActionMap.find(action);
    if (iter != mActionMap.end()) {
        iter->second.emplace_back(activityName);
    } else {
        mActionMap.emplace(action, vector<string>{activityName});
    }
}

bool IntentAction::getFirstTargetByAction(const string& action, string& outTarget) {
    const auto iter = mActionMap.find(action);
    if (iter != mActionMap.end()) {
        outTarget = iter->second[0];
        return true;
    } else {
        return false;
    }
}

bool IntentAction::getTargetsByAction(const string& action, vector<string>& targets) {
    auto iter = mActionMap.find(action);
    if (iter != mActionMap.end()) {
        targets = iter->second;
        return true;
    } else {
        return false;
    }
}

} // namespace am
} // namespace os