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

#include "app/Intent.h"

namespace os {
namespace app {

class Context {
public:
    Context() = default;
    virtual ~Context() = default;
    virtual std::string getPackageName() = 0;
    virtual void startActivity(const Intent& intent) = 0;
    virtual void startActivityForResult(const Intent& intent, int requestCode) = 0;
    virtual void startService(const Intent& intent) = 0;

    virtual void setIntent(const Intent& intent) = 0;
    virtual const Intent& getIntent() = 0;
};

class ContextWrapper : public Context {
public:
    std::shared_ptr<Context> getContext();
    void attachBaseContext(std::shared_ptr<Context> base);
    std::string getPackageName();

    void startActivity(const Intent& intent);
    void startActivityForResult(const Intent& intent, int requestCode);
    void startService(const Intent& intent);

    void setIntent(const Intent& intent);
    const Intent& getIntent();

protected:
    std::shared_ptr<Context> mBase;
};

} // namespace app
} // namespace os