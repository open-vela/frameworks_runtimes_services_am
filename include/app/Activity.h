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

#include "app/Context.h"

namespace os {

namespace wm {
class BaseWindow;
class WindowManager;
} // namespace wm

namespace app {

class ActivityClientRecord;
class ApplicationThreadStub;

/**
 * @class Activity
 * @brief Represents a crucial component of an Android app.
 *
 * Activity is a component used to represent a user interface in Android.
 */
class Activity : public ContextWrapper {
public:
    /**
     * @brief Default constructor for Activity.
     */
    Activity() = default;
    /**
     * @brief Default Destructor for Activity.
     */
    virtual ~Activity() = default;
    /**
     * @brief Lifecycle method called when the Activity is created.
     */
    virtual void onCreate() = 0;
    /**
     * @brief Lifecycle method called when the Activity becomes visible and starts interacting with
     * the user.
     */
    virtual void onStart() = 0;
    /**
     * @brief Lifecycle method called when the Activity is fully visible and interactive.
     */
    virtual void onResume() = 0;
    /**
     * @brief Lifecycle method called when the Activity is paused and no longer in the foreground.
     */
    virtual void onPause() = 0;
    /**
     * @brief Lifecycle method called when the Activity is no longer visible and stops interacting
     * with the user.
     */
    virtual void onStop() = 0;
    /**
     * @brief Lifecycle method called when the Activity is about to be destroyed.
     */
    virtual void onDestroy() = 0;

    /**
     * @brief Called when the Activity is restarted after being stopped.
     */
    virtual void onRestart(){};
    /**
     * @brief Called when the Activity receives a new Intent.
     *
     * @param[in] intent The new Intent that was received.
     */
    virtual void onNewIntent(const Intent& intent){};
    /**
     * @brief Called when the Activity receives a result from another Activity.
     *
     * @param[in] requestCode The code identifying the request.
     * @param[in] resultCode The result code of the operation.
     * @param[in] resultData The data returned from the operation.
     */
    virtual void onActivityResult(const int requestCode, const int resultCode,
                                  const Intent& resultData){};
    /**
     * @brief Called when the back button is pressed.
     */
    virtual void onBackPressed() {
        finish();
    }
    /**
     * @brief Called when the Activity receives an Intent during its lifecycle.
     *
     * @param[in] intent The Intent that was received.
     */
    virtual void onReceiveIntent(const Intent& intent){};

    /**
     * @brief Sets the result for the Activity and finishes it.
     *
     * @param[in] resultCode The result code indicating the outcome
     * @param[in] resultData The data to return to the calling Activity.
     */
    void setResult(const int resultCode, const std::shared_ptr<Intent>& resultData);
    /**
     * @brief Finishes the Activity and removes it from the stack.
     */
    void finish();
    /**
     * @brief Moves the Activity to the background.
     *
     * @param[in] nonRoot Whether to consider root status when moving to the background. Default is
     * true.
     * @return True if the Activity was successfully moved to the background, false otherwise.
     */
    bool moveToBackground(bool nonRoot = true);
    /**
     * @brief Gets the window associated with the Activity.
     *
     * @return A shared pointer to the BaseWindow object associated with this Activity.
     */
    std::shared_ptr<::os::wm::BaseWindow> getWindow() {
        return mWindow;
    }

private:
    friend class ActivityClientRecord;
    friend class ApplicationThreadStub;
    /**
     * @brief Attaches the Activity to a given Context.
     *
     * @param[in] context The Context to attach the Activity to.
     * @return An integer representing the result of the attachment process.
     */
    int attach(std::shared_ptr<Context> context);
    /**
     * @brief Performs the creation lifecycle step for the Activity.
     *
     * @return True if the creation was successful, false otherwise.
     */
    bool performCreate();
    /**
     * @brief Performs the start lifecycle step for the Activity.
     *
     * @return True if the start was successful, false otherwise.
     */
    bool performStart();
    /**
     * @brief Performs the resume lifecycle step for the Activity.
     *
     * @return True if the resume was successful, false otherwise.
     */
    bool performResume();
    /**
     * @brief Performs the pause lifecycle step for the Activity.
     *
     * @return True if the pause was successful, false otherwise.
     */
    bool performPause();
    /**
     * @brief Performs the stop lifecycle step for the Activity.
     *
     * @return True if the stop was successful, false otherwise.
     */
    bool performStop();
    /**
     * @brief Performs the destroy lifecycle step for the Activity.
     *
     * @return True if the destroy was successful, false otherwise.
     */
    bool performDestroy();

private:
    int mResultCode;                     /**< The result code returned from the activity. */
    std::shared_ptr<Intent> mResultData; /**< The result data returned from the activity. */
    std::shared_ptr<::os::wm::BaseWindow> mWindow; /**< The window associated with this activity. */
};

} // namespace app
} // namespace os
