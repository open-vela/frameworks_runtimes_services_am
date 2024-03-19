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

#include <binder/Parcel.h>
#include <binder/PersistableBundle.h>
#include <binder/Status.h>

#include <string>

namespace os {
namespace app {

class Intent : public android::Parcelable {
public:
    std::string mTarget;
    std::string mAction;
    std::string mData;
    uint32_t mFlag;
    /** PersistableBundle,a mapping from String values to various types */
    android::os::PersistableBundle mExtra;

    enum {
        NO_FLAG = 0,
        FLAG_ACTIVITY_NEW_TASK = 1,
        FLAG_ACTIVITY_SINGLE_TOP = 2,
        FLAG_ACTIVITY_CLEAR_TOP = 4,
        FLAG_ACTIVITY_CLEAR_TASK = 8,
    };

    Intent() : mFlag(NO_FLAG){};
    ~Intent() = default;

    Intent(const std::string& target) : mTarget(target), mFlag(NO_FLAG) {}
    void setTarget(const std::string& target);
    void setAction(const std::string& action);
    void setData(const std::string& data);
    void setFlag(const int32_t flag);
    void setBundle(const android::os::PersistableBundle& extra);

    android::status_t readFromParcel(const android::Parcel* parcel) final;
    android::status_t writeToParcel(android::Parcel* parcel) const final;

public:
    /****************** target definition *****************/
    static const std::string TARGET_PREFLEX;
    static const std::string TARGET_ACTIVITY_TOPRESUME;
    static const std::string TARGET_APPLICATION_FOREGROUND;
    static const std::string TARGET_APPLICATION_HOME;
    /******************************************************/

    /****************** action definition *****************/
    static const std::string ACTION_BOOT_READY;
    static const std::string ACTION_BOOT_COMPLETED;
    static const std::string ACTION_HOME;
    static const std::string ACTION_BOOT_GUIDE;
    // broadcast
    static const std::string BROADCAST_APP_START;
    static const std::string BROADCAST_APP_EXIT;
    static const std::string BROADCAST_TOP_ACTIVITY;
    /******************************************************/
}; // class Intent

} // namespace app
} // namespace os
