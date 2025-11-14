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

#include "XMSConfig.h"
#include "app/ActivityManager.h"
#include "app/Application.h"
#include "app/Context.h"

namespace os {
namespace app {

/**
 * @brief ContextImpl class that implements the Context interface.
 */
class ContextImpl : public Context {
public:
    /**
     * @brief Constructor for the ContextImpl class.
     *
     * @param[in] app The application associated with this context.
     * @param[in] componentName The component name (e.g., Activity or Service).
     * @param[in] token The IBinder token associated with this context.
     * @param[in] loop The event loop associated with this context.
     */
    ContextImpl(const Application* app, const string& componentName, const sp<IBinder>& token,
                UvLoop* loop);
    /**
     * @brief Get the application associated with this context.
     *
     * @return A pointer to the Application instance.
     */
    const Application* getApplication() const override;
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
    UvLoop* getMainLoop() const override;
    /**
     * @brief Get the current event loop for the application.
     *
     * @return A pointer to the UvLoop instance representing the current event loop.
     */
    UvLoop* getCurrentLoop() const override;
    /**
     * @brief Get the token associated with this context.
     *
     * @return A reference to the IBinder token.
     */
    const sp<IBinder>& getToken() const override;
    /**
     * @brief Get the Activity Manager for managing activities in the context.
     *
     * @return A reference to the ActivityManager instance.
     */
    ActivityManager& getActivityManager() override;
    /**
     * @brief Get the Window Manager for managing window operations in the context.
     *
     * @return A pointer to the WindowManager instance.
     */
    ::os::wm::WindowManager* getWindowManager() override;
    /**
     * @brief Static method to create an activity context.
     *
     * This method creates a ContextImpl for an activity context.
     *
     * @param[in] app The application associated with the context.
     * @param[in] componentName The component name for the activity.
     * @param[in] token The IBinder token associated with the activity.
     * @param[in] loop The event loop for the activity.
     * @return A shared pointer to a ContextImpl instance for an activity.
     */
    static std::shared_ptr<Context> createActivityContext(const Application* app,
                                                          const string& componentName,
                                                          const sp<IBinder>& token, UvLoop* loop);
    /**
     * @brief Static method to create a service context.
     *
     * This method creates a ContextImpl for a service context.
     *
     * @param[in] app The application associated with the context.
     * @param[in] componentName The component name for the service.
     * @param[in] token The IBinder token associated with the service.
     * @param[in] loop The event loop for the service.
     * @return A shared pointer to a ContextImpl instance for a service.
     */
    static std::shared_ptr<Context> createServiceContext(const Application* app,
                                                         const string& componentName,
                                                         const sp<IBinder>& token, UvLoop* loop);
    /**
     * @brief Static method to create a dialog context.
     *
     * This method creates a ContextImpl for a dialog context.
     *
     * @param[in] app The application associated with the context.
     * @param[in] componentName The component name for the dialog.
     * @param[in] token The IBinder token associated with the dialog.
     * @param[in] loop The event loop for the dialog.
     * @return A shared pointer to a ContextImpl instance for a dialog.
     */
    static std::shared_ptr<Context> createDialogContext(const Application* app,
                                                        const string& componentName,
                                                        const sp<IBinder>& token, UvLoop* loop);
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
     * @brief Stop the application associated with this context.
     *
     * @return A status code indicating the result of stopping the application.
     */
    int32_t stopApplication() override;
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
    void setIntent(const Intent& intent) override;
    /**
     * @brief Get the current intent for the context.
     *
     * @return A reference to the current intent.
     */
    const Intent& getIntent() override;

    template <typename Func>
    auto executeAmMethod(Func&& func) -> decltype(func()) {
        if (!xmsLiteMode()) {
            return func();
        }

        mApp->getMainLoop()->postTask([func = std::forward<Func>(func)]() { func(); });

        if constexpr (!std::is_same_v<decltype(func()), void>) {
            return decltype(func()){};
        }
    }

public:
    const Application* mApp;     /**< The application associated with this context. */
    const string mComponentName; /**< The component name (e.g., activity or service). */
    const sp<IBinder> mToken;    /**< The binder token associated with this context. */
    UvLoop* mLoop;               /**< The event loop associated with this context. */

    ActivityManager mAm; /**< The Activity Manager for managing activities. */
    Intent mIntent;      /**< The intent associated with this context. */
};

} // namespace app
} // namespace os
