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

class Dialog : public ContextWrapper {
public:
    Dialog(const std::shared_ptr<Context>& context);
    ~Dialog();
    static std::shared_ptr<Dialog> createDialog(Context* context);

    void setLayout(wm::LayoutParams& layout);
    wm::LayoutParams getLayout();
    void* getRoot();
    void show();
    void hide();
    void setRect(int32_t left, int32_t top, int32_t width, int32_t height);

private:
    std::shared_ptr<::os::wm::BaseWindow> mDialog;
};

} // namespace app
} // namespace os
