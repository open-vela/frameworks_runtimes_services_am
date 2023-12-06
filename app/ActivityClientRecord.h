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

#include <app/Activity.h>

namespace os {
namespace app {

class ActivityClientRecord {
public:
    ActivityClientRecord(const string& name, const std::shared_ptr<Activity> activity);
    ~ActivityClientRecord();

    /** The status is part of the ServiceRecord inside */
    enum {
        ERROR = -1,
        INIT = 0,
        CREATING,
        CREATED,
        STARTING,
        STARTED,
        RESUMING,
        RESUMED,
        PAUSING,
        PAUSED,
        STOPPING,
        STOPPED,
        DESTROYING,
        DESTROYED,
    };

    void reportActivityStatus(const int32_t status);
    int32_t getStatus();
    void onActivityResult(const int requestCode, const int resultCode, const Intent& resultData);

    int onCreate(const Intent& intent);
    int onStart(const Intent& intent);
    int onResume(const Intent& intent);
    int onPause();
    int onStop();
    int onDestroy();

private:
    const string mActivityName;
    std::shared_ptr<Activity> mActivity;
    int32_t mStatus;
};

} // namespace app
} // namespace os