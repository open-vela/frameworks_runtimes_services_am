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

#include "app/ContextImpl.h"

namespace os {
namespace app {

using std::string;

ContextImpl::ContextImpl(const Application* app, const sp<IBinder>& token, UvLoop* loop)
      : mApp(app), mToken(token), mLoop(loop) {}

std::shared_ptr<Context> ContextImpl::createActivityContext(const Application* app,
                                                            const sp<IBinder>& token,
                                                            UvLoop* loop) {
    return std::make_shared<ContextImpl>(app, token, loop);
}

std::shared_ptr<Context> ContextImpl::createServiceContext(const Application* app,
                                                           const sp<IBinder>& token, UvLoop* loop) {
    return std::make_shared<ContextImpl>(app, token, loop);
}

const Application* ContextImpl::getApplication() const {
    return mApp;
}

string ContextImpl::getPackageName() {
    return mApp->getPackageName();
}

UvLoop* ContextImpl::getMainLoop() const {
    return mApp->getMainLoop();
}

UvLoop* ContextImpl::getCurrentLoop() const {
    return mLoop;
}

const sp<IBinder>& ContextImpl::getToken() const {
    return mToken;
}

ActivityManager& ContextImpl::getActivityManager() {
    return mAm;
}

void ContextImpl::startActivity(const Intent& intent) {
    mAm.startActivity(mToken, intent, ActivityManager::NO_REQUEST);
}

void ContextImpl::startActivityForResult(const Intent& intent, int32_t requestCode) {
    mAm.startActivity(mToken, intent, requestCode);
}

void ContextImpl::startService(const Intent& intent) {
    mAm.startService(intent);
}

void ContextImpl::stopService(const Intent& intent) {
    mAm.stopService(intent);
}

int ContextImpl::bindService(const Intent& intent, const sp<IServiceConnection>& conn) {
    return mAm.bindService(mToken, intent, conn);
}

void ContextImpl::unbindService(const sp<IServiceConnection>& conn) {
    return mAm.unbindService(conn);
}

int32_t ContextImpl::sendBroadcast(const Intent& intent) {
    return mAm.sendBroadcast(intent);
}

int32_t ContextImpl::registerReceiver(const std::string& action,
                                      const sp<IBroadcastReceiver>& receiver) {
    return mAm.registerReceiver(action, receiver);
}

void ContextImpl::unregisterReceiver(const sp<IBroadcastReceiver>& receiver) {
    return mAm.unregisterReceiver(receiver);
}

void ContextImpl::setIntent(const Intent& intent) {
    mIntent = intent;
}

const Intent& ContextImpl::getIntent() {
    return mIntent;
}

} // namespace app
} // namespace os
