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
#ifndef CONFIG_AM_INTENT_BUNDLE
#include <binder/PersistableBundle.h>
#endif
#include <binder/Status.h>

#include <string>

namespace os {
namespace app {

/**
 * @class Intent
 * @brief Represents an intent used for application communication and activity management.
 *
 * The `Intent` class is used to describe an action to be performed, the target of the action,
 * the data to be processed, and any flags or extra information related to the intent.
 */
class Intent : public android::Parcelable {
public:
    std::string mTarget; /**< The target of the intent (e.g., an activity or service). */
    std::string mAction; /**< The action to be performed (e.g., ACTION_VIEW). */
    std::string mData;   /**< Data associated with the intent (e.g., a URI). */
    uint32_t mFlag;      /**< Flags that modify the behavior of the intent (e.g.,
                            FLAG_ACTIVITY_NEW_TASK). */
    /** PersistableBundle,a mapping from String values to various types */
#ifndef CONFIG_AM_INTENT_BUNDLE
    android::os::PersistableBundle mExtra; /**< Extra data as a key-value mapping. */
#endif

    /**
     * @enum Flag Constants
     * @brief Intent flags used to define special behavior for the activity or task.
     */
    enum {
        NO_FLAG = 0,                  /**< No flags set. */
        FLAG_ACTIVITY_NEW_TASK = 1,   /**< Start a new task for the activity. */
        FLAG_ACTIVITY_SINGLE_TOP = 2, /**< If the activity is already at the top, its onNewIntent()
                                         method is called, otherwise a new instance is created. */
        FLAG_ACTIVITY_CLEAR_TOP =
                4, /**< If an instance of activity already exists in the task, its onNewIntent()
                      method is called, and clear activities above the current one. */
        FLAG_ACTIVITY_CLEAR_TASK =
                8, /**< Clear the task stack and start a new instance of the activity. */

        FLAG_APP_SWITCH_TASK = 1024, /** only switch task, don't set new intent */
        FLAG_APP_MOVE_BACK,          /**< move app to background, it only works in stopActivity */
    };
    /**
     * @brief Constructor for the Intent class.
     *
     * Initializes the intent with default values.
     */
    Intent() : mFlag(NO_FLAG){};
    /**
     * @brief Default destructor for the Intent class.
     */
    ~Intent() = default;

    /**
     * @brief Constructor that initializes the target of the intent.
     *
     * @param[in] target The target of the intent (e.g., activity or service).
     */
    Intent(const std::string& target) : mTarget(target), mFlag(NO_FLAG) {}
    /**
     * @brief Sets the target of the intent.
     *
     * @param[in] target The target of the intent (e.g., activity or service).
     */
    void setTarget(const std::string& target);
    /**
     * @brief Sets the action of the intent.
     *
     * @param[in] action The action to be performed.
     */
    void setAction(const std::string& action);
    /**
     * @brief Sets the data associated with the intent.
     *
     * @param[in] data The data to be performed, typically in URI format.
     */
    void setData(const std::string& data);
    /**
     * @brief Sets the flags of the intent.
     *
     * @param[in] flag The flags to be set for the intent.
     */
    void setFlag(const int32_t flag);
    /**
     * @brief Sets additional data for the intent as a `PersistableBundle`.
     *
     * @param[in] extra The additional data to be included in the intent.
     */
#ifndef CONFIG_AM_INTENT_BUNDLE
    void setBundle(const android::os::PersistableBundle& extra);
#endif
    /**
     * @brief Reads the intent data from a parcel.
     *
     * This function is used to deserialize the intent from a parcel.
     *
     * @param[in] parcel The parcel containing the serialized intent data.
     * @return android::OK on success and an appropriate error otherwise.
     */
    android::status_t readFromParcel(const android::Parcel* parcel) final;
    /**
     * @brief Writes the intent to a parcel.
     *
     * @param[in] parcel The parcel to which the intent will be written.
     * @return Returns android::OK on success and an appropriate error otherwise.
     */
    android::status_t writeToParcel(android::Parcel* parcel) const final;

public:
    /****************** target definition *****************/
    static const std::string TARGET_PREFLEX; /**< Target for the prefixed application. */
    static const std::string
            TARGET_ACTIVITY_TOPRESUME; /**< Target for the top activity to resume. */
    static const std::string
            TARGET_APPLICATION_FOREGROUND; /**< Target for bringing the app to foreground. */
    static const std::string TARGET_APPLICATION_HOME; /**< Target for the home screen. */
    /******************************************************/

    /****************** action definition *****************/
    static const std::string
            ACTION_BOOT_READY; /**< Action that signals the system is ready to boot.*/
    static const std::string
            ACTION_BOOT_COMPLETED; /**< Action that signals the system has completed booting. */
    static const std::string ACTION_HOME; /**< Action that corresponds to the home button press. */
    static const std::string
            ACTION_BOOT_GUIDE; /**< Action that corresponds to the boot guide (initial setup). */
    static const std::string
            ACTION_BACK_PRESSED; /**< Action that corresponds to the back button press. */
    // broadcast
    static const std::string
            BROADCAST_APP_START; /**< Broadcast action signaling that an app has started. */
    static const std::string
            BROADCAST_APP_EXIT; /**< Broadcast action signaling that an app has exited. */
    static const std::string BROADCAST_TOP_ACTIVITY; /**< Broadcast action for the top activity. */
    /******************************************************/
}; // class Intent

} // namespace app
} // namespace os
