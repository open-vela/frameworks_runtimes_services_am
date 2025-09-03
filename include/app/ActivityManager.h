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

#include <memory>
#include <mutex>
#include <string>

#include "AmsConfig.h"
#include "app/Intent.h"
#include "os/am/IActivityManager.h"
#include "os/app/IApplicationThread.h"
#include "os/app/IServiceConnection.h"

namespace os {
namespace app {

using android::IBinder;
using android::sp;
using os::am::IActivityManager;
using os::app::Intent;
using os::app::IServiceConnection;
using std::string;

/**
 * @class ActivityManager
 * @brief Manages activities and services within the Android application.
 *
 * This class provides various methods to manage the lifecycle of activities,
 * services, and broadcast receivers, as well as communicating with the activity
 * and service managers in the system.
 */
class ActivityManager {
public:
    /**
     * @brief Default constructor for ActivityManager.
     */
    ActivityManager() = default;
    /**
     * @brief Default destructor for ActivityManager.
     */
    ~ActivityManager() = default;

    /**
     * @brief Get the name of the activity manager.
     *
     * @return The name of the activity manager as a constant C-string.
     */
    static inline const char* name() {
        return "activity";
    }

    /** define constant macro */
    static const int NO_REQUEST = -1;    /**< No request code. */
    static const int RESULT_OK = 0;      /**< Result code indicating success. */
    static const int RESULT_CANCEL = -1; /**< Cancelled result code. */

    /** The status is part of AMS inside */
    enum {
        CREATED = 1,    /**< Activity has been created. */
        STARTED = 3,    /**< Activity has started. */
        RESUMED = 5,    /**< Activity has resumed. */
        PAUSED = 7,     /**< Activity has paused. */
        STOPPED = 9,    /**< Activity has stopped. */
        DESTROYED = 11, /**< Activity has been destroyed. */
    };

    /**
     * @brief Attach an application thread to the activity manager.
     *
     * @param[in] app The application thread to attach.
     * @return An integer representing the status of the operation.
     */
    int32_t attachApplication(const sp<os::app::IApplicationThread>& app);
    /**
     * @brief Start an activity.
     *
     * @param[in] token The token for the activity.
     * @param[in] intent The intent to start the activity.
     * @param [in] requestCode The request code for the activity.
     * @return The status of the operation.
     */
    int32_t startActivity(const sp<IBinder>& token, const Intent& intent, int32_t requestCode);
    /**
     * @brief Stop an activity.
     *
     * @param[in] intent The intent representing the activity to be stopped.
     * @param[in] resultCode The result code for the activity stop.
     * @return The status of the operation.
     */
    int32_t stopActivity(const Intent& intent, int32_t resultCode);
    /**
     * @brief Stop a running application.
     *
     * @param[in] token The token of the application to stop.
     * @return The status of the operation.
     */
    int32_t stopApplication(const sp<IBinder>& token);
    /**
     * @brief Finish the specified activity.
     *
     * @param[in] token The token of the activity to finish.
     * @param[in] resultCode The result code of the activity.
     * @param[in] resultData The data associated with the result.
     * @return True if the activity was successfully finished, false otherwise.
     */
    bool finishActivity(const sp<IBinder>& token, int32_t resultCode,
                        const std::shared_ptr<Intent>& resultData);
    /**
     * @brief Move the activity task to the background.
     *
     * @param[in] token The token of the activity.
     * @param[in] nonRoot If true, only non-root activities will be moved to the background.
     * @return The status of the operation.
     */
    bool moveActivityTaskToBackground(const sp<IBinder>& token, bool nonRoot);
    /**
     * @brief Report the status of an activity.
     *
     * @param[in] token The token of the activity.
     * @param[in] status The status to report.
     */
    void reportActivityStatus(const sp<IBinder>& token, int32_t status);
    /**
     * @brief Start a service.
     *
     * @param[in] intent The intent to start the service.
     * @return The status of the operation.
     */
    int32_t startService(const Intent& intent);
    /**
     * @brief Stop a service.
     *
     * @param[in] intent The intent associated with the service.
     * @return The status of the operation.
     */
    int32_t stopService(const Intent& intent);
    /**
     * @brief Report the status of a service.
     *
     * @param[in] token The token of the service.
     * @param[in] status The status to report.
     */
    void reportServiceStatus(const sp<IBinder>& token, int32_t status);
    /**
     * @brief Bind a service.
     *
     * @param[in] token The token for the service.
     * @param[in] intent The intent to bind the service.
     * @param[in] conn The service connection.
     * @return The status of the operation.
     */
    int32_t bindService(const sp<IBinder>& token, const Intent& intent,
                        const sp<IServiceConnection>& conn);
    /**
     * @brief Unbind a service.
     *
     * @param[in] conn The service connection to unbind.
     */
    void unbindService(const sp<IServiceConnection>& conn);
    /**
     * @brief Publish a service.
     *
     * @param[in] token The token of the service.
     * @param[in] serviceHandler The handler for the service.
     */
    void publishService(const sp<IBinder>& token, const sp<IBinder>& serviceHandler);
    /**
     * @brief Stop a service by its token.
     *
     * @param[in] token The token of the service to stop.
     * @return The status of the operation.
     */
    int32_t stopServiceByToken(const sp<IBinder>& token);
    /**
     * @brief Post an intent to the activity manager.
     *
     * @param[in] intent The intent to post.
     * @return The status of the operation.
     */
    int32_t postIntent(const Intent& intent);
    /**
     * @brief Send a broadcast.
     *
     * @param[in] intent The intent for the broadcast.
     * @return The status of the operation.
     */
    int32_t sendBroadcast(const Intent& intent);
    /**
     * @brief Register a receiver for a specific action.
     *
     * @param[in] action The action to listen for.
     * @param[in] receiver The receiver to register.
     * @return The status of the operation.
     */
    int32_t registerReceiver(const std::string& action, const sp<IBroadcastReceiver>& receiver);
    /**
     * @brief Unregister a broadcast receiver.
     *
     * @param[in] receiver The receiver to unregister.
     */
    void unregisterReceiver(const sp<IBroadcastReceiver>& receiver);
    /**
     * @brief Clear an application by its ID.
     *
     * @param[in] app The application to clear.
     * @return The status of the operation.
     */
    int32_t clearApplication(const sp<os::app::IApplicationThread>& app);
    /**
     * @brief Get the activity manager service instance.
     *
     * @return A reference to the activity manager service.
     */
    sp<IActivityManager> getService();

private:
    std::mutex mLock;              /**< Mutex for thread safety. */
    sp<IActivityManager> mService; /**< The activity manager service instance. */
};

} // namespace app
} // namespace os