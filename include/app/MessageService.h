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

/**
 * @class ReplySender
 * @brief A class that sends replies to a message receiver.
 */
class ReplySender {
public:
    /**
     * @brief Constructor that initializes the ReplySender with a sequence number and reply
     * interface.
     *
     * @param[in] seqNo The sequence number of the message.
     * @param[in] reply The interface through which the reply will be sent.
     */
    ReplySender(const int seqNo, const sp<IReply>& reply) : mSeqNo(seqNo), mReply(reply) {}
    /**
     * @brief Sends the reply to the receiver.
     *
     * @param[in] reply The reply message to be sent.
     */
    void reply(const std::string& reply) {
        Status status = mReply->onReply(mSeqNo, reply);
        if (!status.isOk()) {
            ALOGE("Message reply failure. seqNo:%d error:%s", mSeqNo, status.toString8().c_str());
        }
    }

private:
    const int mSeqNo;        /**< The sequence number for the reply message */
    const sp<IReply> mReply; /**< The interface used to send the reply. */
};

/**
 * @class ReplyReceiver
 * @brief A class to receive replies for messages.
 */
class ReplyReceiver : public BnReply {
public:
    /**
     * @brief Abstract method to handle the received reply.
     *
     * @param[in] seqNo The sequence number of the reply.
     * @param[in] data The reply data received.
     */
    virtual void receiveReply(int seqNo, const std::string& data) = 0;

private:
    /**
     * @brief Internal method that is invoked to deliver the reply to the receiver.
     *
     * @param[in] seqNo The sequence number of the reply.
     * @param[in] reply The reply data.
     * @return Returns Status::ok() to indicate success.
     */
    Status onReply(int32_t seqNo, const ::std::string& reply) override {
        receiveReply(seqNo, reply);
        return Status::ok();
    }
};

/**
 * @class MessageChannel
 * @brief A class to send and receive messages through a message channel.
 */
class MessageChannel {
public:
    /**
     * @brief Constructor that initializes the MessageChannel with the service.
     *
     * @param[in] service The `IMessageChannel` service through which messages will be sent.
     */
    MessageChannel(const sp<IMessageChannel>& service) : mService(service) {}
    /**
     * @brief Sends a message and receives a reply synchronously.
     *
     * This method sends a message to the service and waits for a reply.
     *
     * @param[in] request The request message to send.
     * @return The reply message received from the service.
     */
    std::string sendMessageAndReply(const string& request) {
        std::string ret;
        Status status = mService->sendMessageAndReply(request, &ret);
        if (!status.isOk()) {
            ALOGE("sendMessageAndReply error:%s", status.toString8().c_str());
        }
        return ret;
    }
    /**
     * @brief Sends a message with a sequence number and a reply handler.
     *
     * @param[in] request The request message to send.
     * @param[in] seqNo The sequence number of the message.
     * @param[in] reply The reply handler to process the received reply.
     */
    void sendMessage(const std::string& request, const int seqNo, const sp<ReplyReceiver> reply) {
        Status status = mService->sendMessage(request, seqNo, reply);
        if (!status.isOk()) {
            ALOGE("sendMessage error: %s. seqNo(%d)", status.toString8().c_str(), seqNo);
        }
    }

private:
    sp<IMessageChannel> mService; /**< The service used to send and receive messages. */
};

/**
 * @class MessageServiceInterface
 * @brief Interface for message services.
 */
class MessageServiceInterface {
public:
    /**
     * @brief Receives a message and sends a reply synchronously.
     *
     * @param[in] request The request message to receive.
     * @return The reply message to be sent back.
     */
    virtual std::string receiveMessageAndReply(const string& request) = 0;
    /**
     * @brief Receives a message and handles the reply asynchronously.
     *
     * @param[in] request The request message to receive.
     * @param[in] reply A shared pointer to the `ReplySender` object to send a reply.
     */
    virtual void receiveMessage(const std::string& request,
                                const std::shared_ptr<ReplySender>& reply) = 0;
};

/**
 * @class BnMessageService
 * @brief Implementation of the MessageService interface.
 */
class BnMessageService : public BnMessageChannel {
public:
    /**
     * @brief Constructor that initializes the BnMessageService with a service instance.
     *
     * @param[in] service The service instance that implements the message handling logic.
     */
    BnMessageService(MessageServiceInterface* service) : mService(service) {}
    /**
     * @brief Sends a message and receives a reply synchronously.
     *
     * @param[in] request The request message to send.
     * @param[in] reply The string to store the reply message.
     * @return Returns `Status::ok()` on success.
     */
    Status sendMessageAndReply(const std::string& request, std::string* reply) override {
        *reply = mService->receiveMessageAndReply(request);
        return Status::ok();
    }
    /**
     * @brief Sends a message with a sequence number and a reply handler asynchronously.
     *
     * @param[in] request The request message to send.
     * @param[in] seqNo The sequence number of the message.
     * @param[in] reply The reply handler to process the reply.
     * @return Status Returns `Status::ok()` on success.
     */
    Status sendMessage(const std::string& request, int32_t seqNo,
                       const sp<IReply>& reply) override {
        auto replyHandler = std::make_shared<ReplySender>(seqNo, reply);
        mService->receiveMessage(request, replyHandler);
        return Status::ok();
    }

public:
    MessageServiceInterface* mService; /**< The service instance used to handle messages. */
};

/**
 * @class MessageService
 * @brief A service that implements the MessageService interface.
 */
class MessageService : public Service, public MessageServiceInterface {
public:
    /**
     * @brief Constructor that initializes the MessageService with a binder service.
     */
    MessageService() {
        mBinderService = sp<BnMessageService>::make(this);
    };
    /**
     * @brief Abstract method for binding the service with an intent.
     *
     * @param[in] intent The intent used to bind the service.
     */
    virtual void onBindExt(const Intent& intent) = 0;

private:
    /**
     * @brief Binds the service and returns the binder interface.
     *
     * This method is called to bind the service and notify the user. It returns
     * the binder interface for the message service.
     *
     * @param intent The intent used for binding.
     * @return The binder object associated with the service.
     */
    sp<IBinder> onBind(const Intent& intent) override {
        onBindExt(intent);
        return mBinderService;
    }

private:
    sp<BnMessageService>
            mBinderService; /**< The binder service used to communicate with the service. */
};

} // namespace app
} // namespace os