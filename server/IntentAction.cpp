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

#include <pm/PackageManager.h>

namespace os {
namespace am {

using namespace os::pm;

bool IntentAction::getSingleTargetByAction(const string& action, string& outTarget,
                                           const ComponentType type) {
    PackageManager pm;
    std::vector<PackageInfo> allPackages;
    if (0 != pm.getAllPackageInfo(&allPackages)) {
        return false;
    }

    for (auto& packageInfo : allPackages) {
        if (type == COMP_TYPE_ACTIVITY) {
            for (auto& activity : packageInfo.activitiesInfo) {
                for (auto& a : activity.actions) {
                    if (a == action) {
                        outTarget = COMPONENT_NAME_SPLICE(packageInfo.packageName, activity.name);
                        return true;
                    }
                }
            }

        } else if (type == COMP_TYPE_SERVICE) {
            for (auto& service : packageInfo.servicesInfo) {
                for (auto& a : service.actions) {
                    if (a == action) {
                        outTarget = COMPONENT_NAME_SPLICE(packageInfo.packageName, service.name);
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

bool IntentAction::getMultiTargetByAction(const string& action, vector<string>& targetlist,
                                          const ComponentType type) {
    PackageManager pm;
    std::vector<PackageInfo> allPackages;
    if (0 != pm.getAllPackageInfo(&allPackages)) {
        return false;
    }

    string onetarget;
    for (auto& packageInfo : allPackages) {
        if (type == COMP_TYPE_ACTIVITY) {
            for (auto& activity : packageInfo.activitiesInfo) {
                for (auto& a : activity.actions) {
                    if (a == action) {
                        onetarget = COMPONENT_NAME_SPLICE(packageInfo.packageName, activity.name);
                        targetlist.push_back(onetarget);
                    }
                }
            }

        } else if (type == COMP_TYPE_SERVICE) {
            for (auto& service : packageInfo.servicesInfo) {
                for (auto& a : service.actions) {
                    if (a == action) {
                        onetarget = COMPONENT_NAME_SPLICE(packageInfo.packageName, service.name);
                        targetlist.push_back(onetarget);
                    }
                }
            }
        }
    }
    if (!targetlist.empty()) {
        return true;
    }
    return false;
}

} // namespace am
} // namespace os