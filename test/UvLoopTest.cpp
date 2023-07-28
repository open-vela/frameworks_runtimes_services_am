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

#include <app/UvLoop.h>
#include <gtest/gtest.h>
#include <mqueue.h>
#include <unistd.h>

#include <thread>

using namespace os::app;

namespace test {

TEST(UvLoopTest, run) {
    UvLoop looper;
    UvLoop* handler = &looper;
    looper.postTask([handler](void*) {
        EXPECT_EQ(handler->isAlive(), true);
        handler->stop();
    });
    EXPECT_EQ(looper.run(), 0);
    EXPECT_EQ(looper.isAlive(), false);
}

TEST(UvLoopTest, timer) {
    UvLoop looper;

    auto startTime = std::chrono::high_resolution_clock::now();
    UvTimer timer(looper, [startTime](void*) {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        EXPECT_EQ(duration > 999 && duration < 1100, true);
    });
    timer.start(1000, 0);

    EXPECT_EQ(looper.run(), 0);
}

TEST(UvLoop, poll_pipe) {
    UvLoop looper;
    UvLoop* handler = &looper;

    int fd[2];
    ASSERT_NE(pipe(fd), -1);

    UvPoll pollfd(looper, fd[0]);
    pollfd.start(
            UV_READABLE,
            [handler](int f, int status, int events, void* data) {
                char buf[128];
                int count = read(f, buf, sizeof(buf));
                buf[count] = 0;
                EXPECT_EQ(strcmp(buf, "UvPoll Test"), 0);
                handler->stop();
            },
            nullptr);
    char buffer[] = "UvPoll Test";
    write(fd[1], buffer, strlen(buffer));
    looper.run();
}

TEST(UvLoop, poll_mqueue) {
    UvLoop looper;
    UvLoop* handler = &looper;

    struct mq_attr mqstat;
    int oflag = O_CREAT | O_RDWR | O_NONBLOCK;
    mqstat.mq_maxmsg = 100;
    mqstat.mq_msgsize = sizeof(int);
    mqstat.mq_flags = 0;
    int fd = mq_open("/eventLoopPoll", oflag, 0666, &mqstat);
    ASSERT_GE(fd, 0);

    UvPoll pollfd(looper, fd);
    pollfd.start(
            UV_READABLE,
            [handler](int f, int status, int events, void* data) {
                int msg;
                mq_receive(f, (char*)&msg, sizeof(int), NULL);
                EXPECT_EQ(msg, 666);
                mq_receive(f, (char*)&msg, sizeof(int), NULL);
                EXPECT_EQ(msg, 999);
                handler->stop();
            },
            nullptr);
    int num = 666;
    EXPECT_EQ(mq_send(fd, (const char*)&num, sizeof(int), 1), 0);
    num = 999;
    EXPECT_EQ(mq_send(fd, (const char*)&num, sizeof(int), 1), 0);
    looper.run();
}

extern "C" int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

} // namespace test