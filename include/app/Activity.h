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

class Activity : public ContextWrapper {
public:
    Activity() = default;
    virtual ~Activity() = default;

    virtual void onCreate() = 0;
    virtual void onStart() = 0;
    virtual void onResume() = 0;
    virtual void onPause() = 0;
    virtual void onStop() = 0;
    virtual void onDestroy() = 0;

    virtual void onRestart(){};
    virtual void onNewIntent(const Intent& intent){};
    virtual void onActivityResult(const int requestCode, const int resultCode,
                                  const Intent& resultData){};
    virtual void onBackPressed() {
        finish();
    }
    virtual void onReceiveIntent(const Intent& intent){};

    void setResult(const int resultCode, const std::shared_ptr<Intent>& resultData);
    void finish();
    bool moveToBackground(bool nonRoot = true);
    std::shared_ptr<::os::wm::BaseWindow> getWindow() {
        return mWindow;
    }

private:
    friend class ActivityClientRecord;
    friend class ApplicationThreadStub;
    int attach(std::shared_ptr<Context> context);
    bool performCreate();
    bool performStart();
    bool performResume();
    bool performPause();
    bool performStop();
    bool performDestroy();

private:
    int mResultCode;
    std::shared_ptr<Intent> mResultData;
    std::shared_ptr<::os::wm::BaseWindow> mWindow;
};

} // namespace app
} // namespace os
