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

#include "app/ActivityManager.h"
#include "app/Intent.h"
#include "app/ServiceConnection.h"
#include "app/UvLoop.h"

namespace os {

namespace wm {
class WindowManager;
}

namespace app {

class Application;

/**
 * @class Context
 * @brief Interface providing methods to interact with the application's context.
 */
class Context {
public:
    /**
     * @brief Default constructor for Context.
     */
    Context() = default;
    /**
     * @brief Default destructor for Context.
     */
    virtual ~Context() = default;
    /**
     * @brief Get the application associated with this context.
     *
     * @return A pointer to the Application instance.
     */
    virtual const Application* getApplication() const = 0;
    /**
     * @brief Get the package name associated with this context.
     *
     * @return A reference to the package name as a string.
     */
    virtual const string& getPackageName() const = 0;
    /**
     * @brief Get the component name associated with this context.
     *
     * @return A reference to the component name as a string.
     */
    virtual const string& getComponentName() const = 0;
    /**
     * @brief Get the main event loop for the application.
     *
     * @return A pointer to the UvLoop instance representing the main event loop.
     */
    virtual UvLoop* getMainLoop() const = 0;
    /**
     * @brief Get the current event loop for the application.
     *
     * @return A pointer to the UvLoop instance representing the current event loop.
     */
    virtual UvLoop* getCurrentLoop() const = 0;
    /**
     * @brief Get the token associated with this context.
     *
     * @return A reference to the IBinder token.
     */
    virtual const sp<IBinder>& getToken() const = 0;
    /**
     * @brief Get the Activity Manager for managing activities in the context.
     *
     * @return A reference to the ActivityManager instance.
     */
    virtual ActivityManager& getActivityManager() = 0;
    /**
     * @brief Get the Window Manager for managing window operations in the context.
     *
     * @return A pointer to the WindowManager instance.
     */
    virtual ::os::wm::WindowManager* getWindowManager() = 0;
    /**
     * @brief Stop the application associated with this context.
     *
     * @return A status code indicating the result of stopping the application.
     */
    virtual int32_t stopApplication() = 0;
    /**
     * @brief Start an activity associated with this context.
     *
     * @param[in] intent The intent describing the activity to start.
     * @return A status code indicating the result of starting the activity.
     */
    virtual int32_t startActivity(const Intent& intent) = 0;
    /**
     * @brief Start an activity for result with the specified intent.
     *
     * @param[in] intent The intent describing the activity to start.
     * @param[in] requestCode The request code for identifying the result.
     * @return A status code indicating the result of starting the activity.
     */
    virtual int32_t startActivityForResult(const Intent& intent, int32_t requestCode) = 0;
    /**
     * @brief Stop the activity associated with this context.
     *
     * @param[in] intent The intent describing the activity to stop.
     * @return A status code indicating the result of stopping the activity.
     */
    virtual int32_t stopActivity(const Intent& intent) = 0;
    /**
     * @brief Start a service associated with this context.
     *
     * @param[in] intent The intent describing the service to start.
     * @return A status code indicating the result of starting the service.
     */
    virtual int32_t startService(const Intent& intent) = 0;
    /**
     * @brief Stop a service associated with this context.
     *
     * @param[in] intent The intent describing the service to stop.
     * @return A status code indicating the result of stopping the service.
     */
    virtual int32_t stopService(const Intent& intent) = 0;
    /**
     * @brief Stop the currently running service in this context.
     *
     * @return A status code indicating the result of stopping the service.
     */
    virtual int32_t stopService() = 0;
    /**
     * @brief Bind a service to this context.
     *
     * @param[in] intent The intent describing the service to bind.
     * @param[in] conn The connection callback for the service binding.
     * @return A status code indicating the result of binding the service.
     */
    virtual int bindService(const Intent& intent, const sp<IServiceConnection>& conn) = 0;
    /**
     * @brief Unbind a previously bound service in this context.
     *
     * @param[in] conn The connection callback for unbinding the service.
     */
    virtual void unbindService(const sp<IServiceConnection>& conn) = 0;
    /**
     * @brief Post an intent to the current context.
     *
     * @param[in] intent The intent to post.
     * @return A status code indicating the result of posting the intent.
     */
    virtual int32_t postIntent(const Intent& intent) = 0;
    /**
     * @brief Send a broadcast with the specified intent.
     *
     * @param[in] intent The broadcast intent to send.
     * @return A status code indicating the result of sending the broadcast.
     */
    virtual int32_t sendBroadcast(const Intent& intent) = 0;
    /**
     * @brief Register a broadcast receiver for the specified action.
     *
     * @param[in] action The action to register for.
     * @param[in] receiver The broadcast receiver to register.
     * @return A status code indicating the result of registering the receiver.
     */
    virtual int32_t registerReceiver(const std::string& action,
                                     const sp<IBroadcastReceiver>& receiver) = 0;
    /**
     * @brief Unregister a previously registered broadcast receiver.
     *
     * @param[in] receiver The broadcast receiver to unregister.
     */
    virtual void unregisterReceiver(const sp<IBroadcastReceiver>& receiver) = 0;
    /**
     * @brief Set the intent for the context.
     *
     * @param[in] intent The intent to set.
     */
    virtual void setIntent(const Intent& intent) = 0;
    /**
     * @brief Get the current intent for the context.
     *
     * @return A reference to the current intent.
     */
    virtual const Intent& getIntent() = 0;
};

/**
 * @class ContextWrapper
 * @brief ContextWrapper is a wrapper around the Context class, providing additional functionality.
 *
 * This class allows attaching a base context, which can be used for various operations like
 * managing activities, services, broadcasts, and accessing application resources. It provides
 * implementations for all methods of the Context class, delegating to the base context when
 * necessary.
 */
class ContextWrapper : public Context {
public:
    /**
     * @brief Get the base context wrapped by this ContextWrapper.
     *
     * @return A shared pointer to the base Context.
     */
    std::shared_ptr<Context> getContext();
    /**
     * @brief Attach a base context to this ContextWrapper.
     *
     * @param[in] base The base context to attach.
     */
    void attachBaseContext(const std::shared_ptr<Context>& base);
    /**
     * @brief Get the application associated with this context.
     *
     * @return A pointer to the Application instance.
     */
    const Application* getApplication() const;
    /**
     * @brief Get the package name associated with this context.
     *
     * @return A reference to the package name as a string.
     */
    const string& getPackageName() const override;
    /**
     * @brief Get the component name associated with this context.
     *
     * @return A reference to the component name as a string.
     */
    const string& getComponentName() const override;
    /**
     * @brief Get the main event loop for the application.
     *
     * @return A pointer to the UvLoop instance representing the main event loop.
     */
    UvLoop* getMainLoop() const;
    /**
     * @brief Get the current event loop for the application.
     *
     * @return A pointer to the UvLoop instance representing the current event loop.
     */
    UvLoop* getCurrentLoop() const;
    /**
     * @brief Get the token associated with this context.
     *
     * @return A reference to the IBinder token.
     */
    const sp<IBinder>& getToken() const;
    /**
     * @brief Get the Activity Manager for managing activities in the context.
     *
     * @return A reference to the ActivityManager instance.
     */
    ActivityManager& getActivityManager();
    /**
     * @brief Get the Window Manager for managing window operations in the context.
     *
     * @return A pointer to the WindowManager instance.
     */
    ::os::wm::WindowManager* getWindowManager();
    /**
     * @brief Stop the application associated with this context.
     *
     * @return A status code indicating the result of stopping the application.
     */
    int32_t stopApplication() override;
    /**
     * @brief Start an activity associated with this context.
     *
     * @param[in] intent The intent describing the activity to start.
     * @return A status code indicating the result of starting the activity.
     */
    int32_t startActivity(const Intent& intent) override;
    /**
     * @brief Start an activity for result with the specified intent.
     *
     * @param[in] intent The intent describing the activity to start.
     * @param[in] requestCode The request code for identifying the result.
     * @return A status code indicating the result of starting the activity.
     */
    int32_t startActivityForResult(const Intent& intent, int32_t requestCode) override;
    /**
     * @brief Stop the activity associated with this context.
     *
     * @param[in] intent The intent describing the activity to stop.
     * @return A status code indicating the result of stopping the activity.
     */
    int32_t stopActivity(const Intent& intent) override;
    /**
     * @brief Start a service associated with this context.
     *
     * @param[in] intent The intent describing the service to start.
     * @return A status code indicating the result of starting the service.
     */
    int32_t startService(const Intent& intent) override;
    /**
     * @brief Stop a service associated with this context.
     *
     * @param[in] intent The intent describing the service to stop.
     * @return A status code indicating the result of stopping the service.
     */
    int32_t stopService(const Intent& intent) override;
    /**
     * @brief Stop the currently running service in this context.
     *
     * @return A status code indicating the result of stopping the service.
     */
    int32_t stopService() override;
    /**
     * @brief Bind a service to this context.
     *
     * @param[in] intent The intent describing the service to bind.
     * @param[in] conn The connection callback for the service binding.
     * @return A status code indicating the result of binding the service.
     */
    int bindService(const Intent& intent, const sp<IServiceConnection>& conn) override;
    /**
     * @brief Unbind a previously bound service in this context.
     *
     * @param[in] conn The connection callback for unbinding the service.
     */
    void unbindService(const sp<IServiceConnection>& conn) override;
    /**
     * @brief Post an intent to the current context.
     *
     * @param[in] intent The intent to post.
     * @return A status code indicating the result of posting the intent.
     */
    int32_t postIntent(const Intent& intent) override;
    /**
     * @brief Send a broadcast with the specified intent.
     *
     * @param[in] intent The broadcast intent to send.
     * @return A status code indicating the result of sending the broadcast.
     */
    int32_t sendBroadcast(const Intent& intent) override;
    /**
     * @brief Register a broadcast receiver for the specified action.
     *
     * @param[in] action The action to register for.
     * @param[in] receiver The broadcast receiver to register.
     * @return A status code indicating the result of registering the receiver.
     */
    int32_t registerReceiver(const std::string& action,
                             const sp<IBroadcastReceiver>& receiver) override;
    /**
     * @brief Unregister a previously registered broadcast receiver.
     *
     * @param[in] receiver The broadcast receiver to unregister.
     */
    void unregisterReceiver(const sp<IBroadcastReceiver>& receiver) override;
    /**
     * @brief Set the intent for the context.
     *
     * @param[in] intent The intent to set.
     */
    void setIntent(const Intent& intent);
    /**
     * @brief Get the current intent for the context.
     *
     * @return A reference to the current intent.
     */
    const Intent& getIntent();

protected:
    std::shared_ptr<Context> mBase; /**< The base context wrapped by this ContextWrapper. */
};

} // namespace app
} // namespace os
