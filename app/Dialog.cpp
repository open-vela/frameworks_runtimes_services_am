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

    const auto wm = getWindowManager();
    mDialog = wm->newWindow(this);
    mDialog->setType(LayoutParams::TYPE_DIALOG);
    wm->attachIWindow(mDialog);
}

Dialog::~Dialog() {
    const auto wm = getWindowManager();
    if (wm && mDialog) {
        wm->removeWindow(mDialog);
    }
    mDialog = nullptr;
}

LayoutParams Dialog::getLayout() {
    return mDialog->getLayoutParams();
}

void Dialog::setLayout(LayoutParams& layout) {
    mDialog->setLayoutParams(layout);
}

void Dialog::setRect(int32_t left, int32_t top, int32_t width, int32_t height) {
    auto lp = mDialog->getLayoutParams();
    lp.mX = left;
    lp.mY = top;
    lp.mWidth = width;
    lp.mHeight = height;
    mDialog->setLayoutParams(lp);
}

void* Dialog::getRoot() {
    return mDialog->getRoot();
}

void Dialog::show() {
    mDialog->setVisible(true);
}

void Dialog::hide() {
    mDialog->setVisible(false);
}

} // namespace app
} // namespace os
