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

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "app/Activity.h"
#include "app/Service.h"
#include "app/UvLoop.h"
#include "os/app/IApplicationThread.h"

namespace os {

namespace wm {
class WindowManager;
}

namespace app {

using android::IBinder;
using android::sp;
using std::map;
using std::string;
using std::vector;

/**
 * @brief Type alias for a function that creates an Activity instance.
 */
using CreateActivityFunc = std::function<Activity*(void)>;
/**
 * @brief Type alias for a function that creates a Service instance.
 */
using CreateServiceFunc = std::function<Service*(void)>;

class ActivityClientRecord;
class ServiceClientRecord;
class ApplicationThreadStub;

#define REGISTER_ACTIVITY(classname) \
    registerActivity(#classname, []() -> os::app::Activity* { return new classname; });

#define REGISTER_SERVICE(classname) \
    registerService(#classname, []() -> os::app::Service* { return new classname; });

/**
 * @class Application
 * @brief Represents an application that manages activities, services, and other components.
 *
 * This class provides an interface for lifecycle management, including creation,
 * foreground, background, and destruction of the application. It also manages the
 * registration and creation of activities and services, as well as handling intents.
 */
class Application {
public:
    /**
     * @brief Constructor for the Application class.
     */
    Application();
    /**
     * @brief Destructor for the Application class.
     */
    virtual ~Application();
    /**
     * @brief Called when the application is created.
     */
    virtual void onCreate() = 0;
    /**
     * @brief Called when the application comes to the foreground.
     */
    virtual void onForeground() = 0;
    /**
     * @brief Called when the application goes to the background.
     */
    virtual void onBackground() = 0;
    /**
     * @brief Called when the application is destroyed.
     */
    virtual void onDestroy() = 0;
    /**
     * @brief Called when the application receives an intent.
     *
     * @param[in] intent The received Intent.
     */
    virtual void onReceiveIntent(const Intent& intent){};
    /**
     * @brief Get the package name of the application.
     *
     * @return The package name as a constant reference to a string.
     */
    const string& getPackageName() const;
    /**
     * @brief Set the package name for the application.
     *
     * @param[in] name The package name to set.
     */
    void setPackageName(const string& name);
    /**
     * @brief Get the UID of the application.
     *
     * @return The UID of the application.
     */
    int getUid() {
        return mUid;
    }
    /**
     * @brief Set the main loop for the application.
     *
     * @param[in] loop The event loop to set.
     */
    void setMainLoop(UvLoop* loop) {
        mMainLoop = loop;
    }
    /**
     * @brief Get the main loop of the application.
     *
     * @return The event loop associated with the application.
     */
    UvLoop* getMainLoop() const {
        return mMainLoop;
    }
    /**
     * @brief Check if the application is the system UI.
     *
     * @return True if the application is the system UI, false otherwise.
     */
    bool isSystemUI() const;
    /**
     * @brief Register an activity with the application.
     *
     * @param[in] name The name of the activity class to register.
     * @param[in] createFunc The function used to create the activity. @see CreateActivityFunc
     */
    void registerActivity(const string& name, const CreateActivityFunc& createFunc);
    /**
     * @brief Register a service with the application.
     *
     * @param[in] name The name of the service class to register.
     * @param[in] createFunc The function used to create the service. @see CreateServiceFunc
     */
    void registerService(const string& name, const CreateServiceFunc& createFunc);
    /**
     * @brief Get the window manager associated with the application.
     *
     * @return A pointer to the window manager.
     */
    ::os::wm::WindowManager* getWindowManager();

private:
    friend class ApplicationThread;
    friend class ApplicationThreadStub;
    /**
     * @brief Create an activity based on the provided name.
     *
     * @param[in] name The name of the activity to create.
     * @return A shared pointer to the created activity.
     */
    std::shared_ptr<Activity> createActivity(const string& name);
    /**
     * @brief Create a service based on the provided name.
     *
     * @param[in] name The name of the service to create.
     * @return A shared pointer to the created service.
     */
    std::shared_ptr<Service> createService(const string& name);
    /**
     * @brief Add an activity to the list of existing activities.
     *
     * @param[in] token The token of the activity.
     * @param[in] activity The activity client record to add.
     */
    void addActivity(const sp<IBinder>& token,
                     const std::shared_ptr<ActivityClientRecord>& activity);
    /**
     * @brief Find an activity by its token.
     *
     * @param[in] token The token of the activity to find.
     * @return A shared pointer to the activity client record, or nullptr if not found.
     */
    std::shared_ptr<ActivityClientRecord> findActivity(const sp<IBinder>& token);
    /**
     * @brief Delete an activity by its token.
     *
     * @param[in] token The token of the activity to delete.
     */
    void deleteActivity(const sp<IBinder>& token);
    /**
     * @brief Add a service to the list of existing services.
     *
     * @param[in] service The service client record to add.
     */
    void addService(const std::shared_ptr<ServiceClientRecord>& service);
    /**
     * @brief Find a service by its token.
     *
     * @param[in] token The token of the service to find.
     * @return A shared pointer to the service client record, or nullptr if not found.
     */
    std::shared_ptr<ServiceClientRecord> findService(const sp<IBinder>& token);
    /**
     * @brief Delete a service by its token.
     *
     * @param token The token of the service to delete.
     */
    void deleteService(const sp<IBinder>& token);
    /**
     * @brief Clear all activities and services from the application.
     */
    void clearActivityAndService();

private:
    map<sp<IBinder>, std::shared_ptr<ActivityClientRecord>>
            mExistActivities; /**< Map of existing activities by token. */
    vector<std::shared_ptr<ServiceClientRecord>> mExistServices; /**< List of existing services. */
    string mPackageName; /**< The package name of the application. */
    map<string, CreateActivityFunc>
            mActivityMap; /**< Map of activity names to their creation functions. */
    map<string, CreateServiceFunc>
            mServiceMap; /**< Map of service names to their creation functions. */
    int mUid;            /**< The UID of the application. Reserved field, currently not used*/
    pid_t mPid;          /**< The PID of the application. Reserved field, currently not used*/
    UvLoop* mMainLoop;   /**< The main event loop of the application. */
    ::os::wm::WindowManager* mWindowManager; /**< Window manager for the application. */
};

} // namespace app
} // namespace os
