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

#include <os/app/BnBroadcastReceiver.h>

namespace os {
namespace app {

using android::binder::Status;

/**
 * @class BroadcastReceiver
 * @brief Abstract class for handling broadcast intents.
 */
class BroadcastReceiver : public BnBroadcastReceiver {
public:
    /**
     * @brief Called when a broadcast message is received.
     *
     * @param[in] intent The broadcast Intent received.
     */
    virtual void onReceive(const Intent& intent) = 0;

private:
    /**
     * @brief Receives the broadcast and forwards it to `onReceive`.
     *
     * @param[in] intent The broadcast Intent received.
     * @return The status of the broadcast reception (always returns Status::ok()).
     */
    Status receiveBroadcast(const Intent& intent) override {
        onReceive(intent);
        return Status::ok();
    }
};

} // namespace app
} // namespace os