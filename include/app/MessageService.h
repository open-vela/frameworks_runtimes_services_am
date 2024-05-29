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

#include <app/Service.h>
#include <os/app/BnMessageChannel.h>
#include <os/app/BnReply.h>
#include <os/app/IMessageChannel.h>
#include <os/app/IReply.h>

#include "app/Logger.h"

namespace os {
namespace app {

using android::IBinder;
using android::sp;
using android::binder::Status;
using os::app::IReply;

class ReplySender {
public:
    ReplySender(const int seqNo, const sp<IReply>& reply) : mSeqNo(seqNo), mReply(reply) {}
    void reply(const std::string& reply) {
        Status status = mReply->onReply(mSeqNo, reply);
        if (!status.isOk()) {
            ALOGE("Message reply failure. seqNo:%d error:%s", mSeqNo, status.toString8().c_str());
        }
    }

private:
    const int mSeqNo;
    const sp<IReply> mReply;
};

class ReplyReceiver : public BnReply {
public:
    virtual void receiveReply(int seqNo, const std::string& data) = 0;

private:
    Status onReply(int32_t seqNo, const ::std::string& reply) override {
        receiveReply(seqNo, reply);
        return Status::ok();
    }
};

class MessageChannel {
public:
    MessageChannel(const sp<IMessageChannel>& service) : mService(service) {}

    std::string sendMessageAndReply(const string& request) {
        std::string ret;
        Status status = mService->sendMessageAndReply(request, &ret);
        if (!status.isOk()) {
            ALOGE("sendMessageAndReply error:%s", status.toString8().c_str());
        }
        return ret;
    }

    void sendMessage(const std::string& request, const int seqNo, const sp<ReplyReceiver> reply) {
        Status status = mService->sendMessage(request, seqNo, reply);
        if (!status.isOk()) {
            ALOGE("sendMessage error: %s. seqNo(%d)", status.toString8().c_str(), seqNo);
        }
    }

private:
    sp<IMessageChannel> mService;
};

class MessageServiceInterface {
public:
    virtual std::string receiveMessageAndReply(const string& request) = 0;
    virtual void receiveMessage(const std::string& request,
                                const std::shared_ptr<ReplySender>& reply) = 0;
};

class BnMessageService : public BnMessageChannel {
public:
    BnMessageService(MessageServiceInterface* service) : mService(service) {}
    Status sendMessageAndReply(const std::string& request, std::string* reply) override {
        *reply = mService->receiveMessageAndReply(request);
        return Status::ok();
    }

    Status sendMessage(const std::string& request, int32_t seqNo,
                       const sp<IReply>& reply) override {
        auto replyHandler = std::make_shared<ReplySender>(seqNo, reply);
        mService->receiveMessage(request, replyHandler);
        return Status::ok();
    }

public:
    MessageServiceInterface* mService;
};

class MessageService : public Service, public MessageServiceInterface {
public:
    MessageService() {
        mBinderService = sp<BnMessageService>::make(this);
    };

    virtual void onBindExt(const Intent& intent) = 0;

private:
    // we need to return the bindService, and notify the user of bind.
    sp<IBinder> onBind(const Intent& intent) override {
        onBindExt(intent);
        return mBinderService;
    }

private:
    sp<BnMessageService> mBinderService;
};

} // namespace app
} // namespace os