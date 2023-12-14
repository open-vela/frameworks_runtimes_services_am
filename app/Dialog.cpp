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

#include "app/Dialog.h"

#include <BaseWindow.h>
#include <WindowManager.h>
#include <wm/LayoutParams.h>

#include "app/ContextImpl.h"

namespace os {
namespace app {

using namespace ::os::wm;

std::shared_ptr<Dialog> Dialog::createDialog(Context* context) {
    android::sp<IBinder> token = new android::BBinder();
    auto ctx = ContextImpl::createDialogContext(context->getApplication(), "Dialog", token,
                                                context->getMainLoop());
    return std::make_shared<Dialog>(ctx);
}

Dialog::Dialog(const std::shared_ptr<Context>& context) {
    attachBaseContext(context);
    const auto wm = getWindowManager()->getService();
    wm->addWindowToken(getToken(), LayoutParams::TYPE_DIALOG, 0);
    mDialog = getWindowManager()->newWindow(this);
}

Dialog::~Dialog() {
    const auto wm = getWindowManager();
    if (wm && mDialog) {
        wm->removeWindow(mDialog);
    }
}

LayoutParams Dialog::getLayout() {
    return mDialog->getLayoutParams();
}

void Dialog::setLayout(LayoutParams& layout) {
    layout.mType = LayoutParams::TYPE_DIALOG;
    mDialog->setLayoutParams(layout);
    getWindowManager()->attachIWindow(mDialog);
}

void* Dialog::getRoot() {
    return mDialog->getRoot();
}

void Dialog::show() {
    getWindowManager()->getService()->updateWindowTokenVisibility(getToken(),
                                                                  LayoutParams::WINDOW_VISIBLE);
}

void Dialog::hide() {
    getWindowManager()->getService()->updateWindowTokenVisibility(getToken(),
                                                                  LayoutParams::WINDOW_INVISIBLE);
}

} // namespace app
} // namespace os