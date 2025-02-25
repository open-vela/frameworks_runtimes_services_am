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
class LayoutParams;
} // namespace wm

namespace app {

/**
 * @class Dialog
 * @brief Represents a dialog window in the application context.
 */
class Dialog : public ContextWrapper {
public:
    /**
     * @brief Constructor for the Dialog class.
     *
     * @param[in] context A shared pointer to the Context associated with this Dialog.
     */
    Dialog(const std::shared_ptr<Context>& context);
    /**
     * @brief Destructor for the Dialog class.
     */
    ~Dialog();
    /**
     * @brief Static method to create a Dialog instance.
     *
     * @param context The context in which the dialog will be created.
     * @return A shared pointer to the created Dialog instance.
     */
    static std::shared_ptr<Dialog> createDialog(Context* context);
    /**
     * @brief Set the layout parameters for the dialog.
     *
     * This method allows setting the layout parameters for the dialog's window. The layout
     * parameters include things like width, height, gravity, etc.
     *
     * @param layout A reference to a LayoutParams object containing the desired layout parameters.
     */
    void setLayout(wm::LayoutParams& layout);
    /**
     * @brief Get the current layout parameters of the dialog.
     *
     * @return A LayoutParams object containing the current layout parameters of the dialog.
     */
    wm::LayoutParams getLayout();
    /**
     * @brief Get the root view of the dialog.
     */
    void* getRoot();
    /**
     * @brief Show the dialog.
     */
    void show();
    /**
     * @brief Hide the dialog.
     */
    void hide();
    /**
     * @brief Set the position and size of the dialog.
     *
     * This method sets the position (left and top) and size (width and height) of the dialog
     * window.
     *
     * @param[in] left The left position of the dialog window.
     * @param[in] top The top position of the dialog window.
     * @param[in] width The width of the dialog window.
     * @param[in] height The height of the dialog window.
     */
    void setRect(int32_t left, int32_t top, int32_t width, int32_t height);

private:
    std::shared_ptr<::os::wm::BaseWindow>
            mDialog; /**< A shared pointer to the BaseWindow instance representing the dialog. */
};

} // namespace app
} // namespace os
