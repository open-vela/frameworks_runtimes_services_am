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

#include "app/Intent.h"

#include "ParcelUtils.h"

namespace os {
namespace app {

using namespace android;

/****************** action definition *****************/
const std::string Intent::ACTION_HOME("action.system.home");
/******************************************************/

void Intent::setTarget(const std::string& target) {
    mTarget = target;
}

void Intent::setAction(const std::string& action) {
    mAction = action;
}

void Intent::setData(const std::string& data) {
    mData = data;
}

void Intent::setFlag(const int32_t flag) {
    mFlag = flag;
}

void Intent::setBundle(const android::os::PersistableBundle& extra) {
    mExtra = extra;
}

status_t Intent::readFromParcel(const Parcel* parcel) {
    SAFE_PARCEL(parcel->readUtf8FromUtf16, &mTarget);
    SAFE_PARCEL(parcel->readUtf8FromUtf16, &mAction);
    SAFE_PARCEL(parcel->readUtf8FromUtf16, &mData);
    SAFE_PARCEL(parcel->readUint32, &mFlag);
    SAFE_PARCEL(mExtra.readFromParcel, parcel);
    return android::OK;
}

status_t Intent::writeToParcel(Parcel* parcel) const {
    SAFE_PARCEL(parcel->writeUtf8AsUtf16, mTarget);
    SAFE_PARCEL(parcel->writeUtf8AsUtf16, mAction);
    SAFE_PARCEL(parcel->writeUtf8AsUtf16, mData);
    SAFE_PARCEL(parcel->writeUint32, mFlag);
    SAFE_PARCEL(mExtra.writeToParcel, parcel);
    return android::OK;
}

} // namespace app
} // namespace os
