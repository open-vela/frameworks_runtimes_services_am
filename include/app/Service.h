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

#include <string>

#include "app/Context.h"
#include "os/app/IServiceConnection.h"

namespace os {
namespace app {

using os::app::IServiceConnection;

class ServiceClientRecord;

/**
 * @class Service
 * @brief Base class for implementing a service that runs in the background.
 */
class Service : public ContextWrapper {
public:
    /**
     * @brief Constructs a Service object.
     */
    Service();
    /**
     * @brief Default destructor for Service.
     */
    virtual ~Service() = default;
    /**
     * @brief Called when the service is created.
     */
    virtual void onCreate() = 0;
    /**
     * @brief Called when the service receives a start command.
     *
     * @param[in] intent The intent that started the service.
     */
    virtual void onStartCommand(const Intent& intent) = 0;
    /**
     * @brief Called when the service is destroyed.
     */
    virtual void onDestroy() = 0;
    /**
     * @brief Called when a client binds to the service.
     *
     * @param[in] intent The intent used for binding the service.
     * @return A binder object that clients can use to interact with the service.
     */
    virtual sp<IBinder> onBind(const Intent& intent);
    /**
     * @brief Called when a client unbinds from the service.
     *
     * @return Returns true if unbind operation was successful, otherwise `false`.
     */
    virtual bool onUnbind();
    /**
     * @brief Called when an intent is received by the service.
     *
     * @param[in] intent The intent that was received.
     */
    virtual void onReceiveIntent(const Intent& intent){};

private:
    friend class ServiceClientRecord;
    /**
     * @brief Binds the service to a client.
     *
     * This function is used internally to bind the service to a client connection. It establishes
     * the communication channel between the service and the client.
     *
     * @param[in] intent The Intent object containing information about the bind request.
     * @param[in] conn The service connection interface to communicate with the client.
     * @return int A status code indicating the result of the bind operation.
     */
    int bind(const Intent& intent, const sp<IServiceConnection>& conn);
    /**
     * @brief Unbinds the service from the client.
     */
    void unbind();

    sp<IBinder>
            mServiceBinder; /**< The binder object that clients use to interact with the service. */
    bool mIsBinded;         /**< Flag indicating whether the service is bound to a client */
};

} // namespace app
} // namespace os