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

#include <uv.h>

#include <optional>
#include <string>

#include "AmsConfig.h"
#include "os/am/BnActivityManager.h"
#include "os/am/IActivityManager.h"
#include "os/app/IBroadcastReceiver.h"
#include "os/wm/BnWindowManager.h"

namespace os {
namespace am {

using android::IBinder;
using android::sp;
using android::binder::Status;
using os::app::IBroadcastReceiver;
using os::app::Intent;
using os::app::IServiceConnection;

class ActivityManagerInner;

/**
 * @class ActivityManagerService
 * @brief Service that manages activities and services in the system.
 */
class ActivityManagerService : public os::am::BnActivityManager {
public:
    /**
     * @brief Constructor to initialize the ActivityManagerService.
     *
     * @param[in] looper The event loop associated with the service.
     */
    ActivityManagerService(uv_loop_t* looper);
    /**
     * @brief Destructor for the ActivityManagerService.
     */
    ~ActivityManagerService();

    /**
     * @brief Attaches an application to the service.
     *
     * @param[in] app The application thread to be attached.
     * @param[in] ret Output parameter for the result of the operation.
     * @return The status of the operation.
     */
    Status attachApplication(const sp<os::app::IApplicationThread>& app, int32_t* ret) override;
    /**
     * @brief Starts an activity.
     *
     * @param[in] token The token associated with the activity.
     * @param[in] intent The intent used to start the activity.
     * @param[in] code The request code associated with the activity.
     * @param[out] ret Output parameter for the result of the operation.
     * @return The status of the operation.
     */
    Status startActivity(const sp<IBinder>& token, const Intent& intent, int32_t code,
                         int32_t* ret) override;
    /**
     * @brief Stops an activity.
     *
     * @param[in] intent The intent used to stop the activity.
     * @param[in] resultCode The result code to return when stopping the activity.
     * @param[out] ret Output parameter for the result of the operation.
     * @return The status of the operation.
     */
    Status stopActivity(const Intent& intent, int32_t resultCode, int32_t* ret) override;
    /**
     * @brief Stops an application.
     *
     * @param[in] token The token associated with the application to stop.
     * @param[out] ret Output parameter for the result of the operation.
     * @return The status of the operation.
     */
    Status stopApplication(const sp<IBinder>& token, int32_t* ret) override;
    /**
     * @brief Finishes an activity.
     *
     * @param[in] token The token associated with the activity.
     * @param[in] resultCode The result code of the activity.
     * @param[in] resultData Optional data to be passed as the result.
     * @param[out] ret Output parameter indicating whether the operation was successful.
     * @return Status The status of the operation.
     */
    Status finishActivity(const sp<IBinder>& token, int32_t resultCode,
                          const std::optional<Intent>& resultData, bool* ret) override;
    /**
     * @brief Moves an activity task to the background.
     *
     * @param[in] token The token associated with the activity.
     * @param[in] nonRoot Whether the activity is not the root activity.
     * @param[out] ret Output parameter indicating whether the operation was successful.
     * @return The status of the operation.
     */
    Status moveActivityTaskToBackground(const sp<IBinder>& token, bool nonRoot, bool* ret) override;
    /**
     * @brief Reports the status of an activity.
     *
     * @param[in] token The token associated with the activity.
     * @param[in] status The status to report for the activity.
     * @return The status of the operation.
     */
    Status reportActivityStatus(const sp<IBinder>& token, int32_t status) override;
    /**
     * @brief Starts a service.
     *
     * @param[in] intent The intent used to start the service.
     * @param[out] ret Output parameter for the result of the operation.
     * @return The status of the operation.
     */
    Status startService(const Intent& intent, int32_t* ret) override;
    /**
     * @brief Stops a service.
     *
     * @param[in] intent The intent used to stop the service.
     * @param[out] ret Output parameter for the result of the operation.
     * @return The status of the operation.
     */
    Status stopService(const Intent& intent, int32_t* ret) override;
    /**
     * @brief Stops a service by its token.
     *
     * @param[in] token The token associated with the service to stop.
     * @param[out] ret Output parameter for the result of the operation.
     * @return The status of the operation.
     */
    Status stopServiceByToken(const sp<IBinder>& token, int32_t* ret) override;
    /**
     * @brief Reports the status of a service.
     *
     * @param[in] token The token associated with the service.
     * @param[in] status The status to report for the service.
     * @return The status of the operation.
     */
    Status reportServiceStatus(const sp<IBinder>& token, int32_t status) override;
    /**
     * @brief Binds a service.
     *
     * @param[in] token The token associated with the service.
     * @param[in] intent The intent used to bind the service.
     * @param[in] conn The service connection used to manage the connection.
     * @param[out] ret Output parameter for the result of the operation.
     * @return The status of the operation.
     */
    Status bindService(const sp<IBinder>& token, const Intent& intent,
                       const sp<IServiceConnection>& conn, int32_t* ret) override;
    /**
     * @brief Unbinds a service.
     *
     * @param[in] conn The service connection used to manage the connection.
     * @return The status of the operation.
     */
    Status unbindService(const sp<IServiceConnection>& conn) override;
    /**
     * @brief Publishes a service.
     *
     * @param[in] token The token associated with the service.
     * @param[in] service The binder object associated with the service.
     * @return The status of the operation.
     */
    Status publishService(const sp<IBinder>& token, const sp<IBinder>& service) override;
    /**
     * @brief Posts an intent to be processed by the system.
     *
     * @param[in] intent The intent to be posted.
     * @param[out] ret Output parameter for the result of the operation.
     * @return The status of the operation.
     */
    Status postIntent(const Intent& intent, int32_t* ret) override;
    /**
     * @brief Sends a broadcast intent.
     *
     * @param[in] intent The broadcast intent to send.
     * @param[out] ret Output parameter for the result of the operation.
     * @return The status of the operation.
     */
    Status sendBroadcast(const Intent& intent, int32_t* ret) override;
    /**
     * @brief Registers a receiver for a specific action.
     *
     * @param[in] action The action to register for.
     * @param[in] receiver The receiver to be registered.
     * @param[out] ret Output parameter for the result of the operation.
     * @return The status of the operation.
     */
    Status registerReceiver(const std::string& action, const sp<IBroadcastReceiver>& receiver,
                            int32_t* ret) override;
    /**
     * @brief Unregisters a previously registered receiver.
     *
     * @param[in] receiver The receiver to be unregistered.
     * @return The status of the operation.
     */
    Status unregisterReceiver(const sp<IBroadcastReceiver>& receiver) override;
    /**
     * @brief clear xms single app resource.
     *
     * @return True if the app clear success, false otherwise.
     */
    Status clearApplication(const sp<os::app::IApplicationThread>& app, int32_t* ret) override;
    /**
     * @brief Dumps the current state of the ActivityManagerService.
     *
     * @param[in] fd The file descriptor to write the dump to.
     * @param[in] args The arguments to filter the dump.
     * @return The status of the dump operation.
     */
    android::status_t dump(int fd, const std::vector<android::String16>& args) override;

    /**
     * @brief Marks the service as ready for operation.
     *
     * This method is called when the service is ready to start and the application can be launched.
     */
    void systemReady();
    /**
     * @brief Sets the window manager for the system.
     *
     * @param[in] wm The window manager service to set.
     */
    void setWindowManager(sp<::os::wm::IWindowManager> wm);

private:
    ActivityManagerInner* mInner; /**< Internal implementation for managing activity operations. */
};

} // namespace am
} // namespace os
