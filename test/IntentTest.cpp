/*
 * Copyright (C) 2025 Xiaomi Corporation
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

#include <app/Intent.h>
#include <gtest/gtest.h>

namespace test {

class IntentTest : public ::testing::Test {
protected:
    os::app::Intent intent;

protected:
    IntentTest(/* args */) = default;
    ~IntentTest() = default;

    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(IntentTest, TestIntent) {
    EXPECT_EQ(intent.mAction, "");
}

TEST_F(IntentTest, TestIntentAction) {
    intent.setAction("test");
    EXPECT_EQ(intent.mAction, "test");
}

TEST_F(IntentTest, TestIntentData) {
    intent.setData("test");
    EXPECT_EQ(intent.mData, "test");
}

TEST_F(IntentTest, TestIntentFlag) {
    intent.setFlag(os::app::Intent::FLAG_ACTIVITY_NEW_TASK);
    EXPECT_EQ(intent.mFlag, os::app::Intent::FLAG_ACTIVITY_NEW_TASK);
}

TEST_F(IntentTest, TestIntentTarget) {
    intent.setTarget("test");
    EXPECT_EQ(intent.mTarget, "test");
}

#ifndef CONFIG_AM_INTENT_BUNDLE
// 模拟测试 Intent::readFromParcel
TEST_F(IntentTest, TestReadFromParcel) {
    // 创建一个 Parcel 对象
    using android::Parcel;
    using android::status_t;
    using android::String16;
    using android::os::PersistableBundle;
    Parcel parcel;

    // 假设我们写入以下数据
    std::string target("com.example.Target");
    std::string action("com.example.action.TEST");
    std::string data("content://testdata");
    uint32_t flag = 1;
    os::app::Intent tmp;
    tmp.setTarget(target);
    tmp.setAction(action);
    tmp.setData(data);
    tmp.setFlag(flag);
    tmp.writeToParcel(&parcel);

    // 创建 Intent 对象并调用 readFromParcel
    // 重置数据指针到开始位置（为了读取数据）
    parcel.setDataPosition(0);
    status_t result = intent.readFromParcel(&parcel);

    // 验证返回值是否是 OK
    ASSERT_EQ(result, android::OK);

    // 验证目标数据是否正确读取
    ASSERT_EQ(intent.mTarget, target);
    ASSERT_EQ(intent.mAction, action);
    ASSERT_EQ(intent.mData, data);
    ASSERT_EQ(intent.mFlag, flag);

    // 验证 mExtra 是否正确读取，假设我们可以通过一些方式验证
    // PersistableBundle 的内容，具体方式视其实现而定
    // ASSERT_EQ(intent.mExtra.someCheck(), expectedValue);
}

// 测试 writeToParcel 方法
TEST_F(IntentTest, TestWriteToParcel) {
    intent.setTarget("com.example.Target");
    intent.setAction("com.example.action.TEST");
    intent.setData("content://testdata");
    intent.setFlag(os::app::Intent::FLAG_ACTIVITY_NEW_TASK);

    // 创建一个 Parcel 对象
    using android::Parcel;
    using android::status_t;
    using android::String16;
    using android::os::PersistableBundle;
    Parcel parcel;

    // 写入数据到 Parcel
    status_t writeStatus = intent.writeToParcel(&parcel);
    ASSERT_EQ(writeStatus, android::OK); // 验证写入是否成功

    // 重置数据指针到开始位置（为了读取数据）
    parcel.setDataPosition(0);

    // 创建一个新的 MyData 对象，用来读取 Parcel 中的数据
    os::app::Intent newData;
    status_t readStatus = newData.readFromParcel(&parcel);
    ASSERT_EQ(readStatus, android::OK); // 验证读取是否成功

    // 验证新对象的数据是否与原始数据一致
    ASSERT_EQ(newData.mTarget, "com.example.Target");
    ASSERT_EQ(newData.mAction, "com.example.action.TEST");
    ASSERT_EQ(newData.mData, "content://testdata");
    ASSERT_EQ(newData.mFlag, os::app::Intent::FLAG_ACTIVITY_NEW_TASK);
}

#endif
} // namespace test