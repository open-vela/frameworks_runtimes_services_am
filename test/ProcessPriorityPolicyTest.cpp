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

#include <gtest/gtest.h>

#include <memory>

#include "ProcessPriorityPolicy.h"

using namespace os::app;

namespace test {

class ProcessPriorityPolicyTest : public ::testing::Test {
protected:
    UvLoop mloop;
    os::am::LowMemoryManager m_lmm;
    std::unique_ptr<os::am::ProcessPriorityPolicy> m_policy;

protected:
    ProcessPriorityPolicyTest(/* args */) {
        m_policy = std::make_unique<os::am::ProcessPriorityPolicy>(&m_lmm);
    }
    ~ProcessPriorityPolicyTest() = default;

    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(ProcessPriorityPolicyTest, init) {
    EXPECT_TRUE(m_lmm.init(&mloop));
}

TEST_F(ProcessPriorityPolicyTest, isOkToLaunch) {
    EXPECT_TRUE(m_lmm.isOkToLaunch());
}

TEST_F(ProcessPriorityPolicyTest, getPID1) {
    auto* pnode = m_policy->get(0);
    EXPECT_EQ(pnode, nullptr);
}

TEST_F(ProcessPriorityPolicyTest, addPID1) {
    auto* pnode = m_policy->add(1, true, os::pm::ProcessPriority::MIDDLE);
    EXPECT_EQ(pnode->appId, 1);
}

TEST_F(ProcessPriorityPolicyTest, addPID2) {
    auto* pnode = m_policy->add(2, true, os::pm::ProcessPriority::MIDDLE);
    EXPECT_EQ(pnode->appId, 2);
}

TEST_F(ProcessPriorityPolicyTest, getPID2) {
    auto* pnode = m_policy->add(3, true, os::pm::ProcessPriority::MIDDLE);
    EXPECT_EQ(pnode->appId, 3);
    auto* pnode2 = m_policy->get(3);
    EXPECT_EQ(pnode2, pnode);
    EXPECT_EQ(pnode2->appId, 3);
}

TEST_F(ProcessPriorityPolicyTest, removePID1) {
    auto* pnode = m_policy->add(4, true, os::pm::ProcessPriority::MIDDLE);
    EXPECT_EQ(pnode->appId, 4);
    m_policy->remove(4);
    auto* pnode2 = m_policy->get(4);
    EXPECT_EQ(pnode2, nullptr);
}

TEST_F(ProcessPriorityPolicyTest, removePID2) {
    auto* pnode = m_policy->add(5, true, os::pm::ProcessPriority::MIDDLE);
    EXPECT_EQ(pnode->appId, 5);
    m_policy->remove(5);
    auto* pnode2 = m_policy->get(5);
    EXPECT_EQ(pnode2, nullptr);
}

TEST_F(ProcessPriorityPolicyTest, pushForeground) {
    auto* pnode = m_policy->add(6, true, os::pm::ProcessPriority::MIDDLE);
    EXPECT_EQ(pnode->appId, 6);
    m_policy->pushForeground(6);
    auto* pnode2 = m_policy->get(6);
    EXPECT_EQ(pnode2, pnode);
    EXPECT_EQ(pnode2->appId, 6);
}

TEST_F(ProcessPriorityPolicyTest, intoBackground) {
    auto* pnode = m_policy->add(7, true, os::pm::ProcessPriority::MIDDLE);
    EXPECT_EQ(pnode->appId, 7);
    m_policy->intoBackground(7);
    auto* pnode2 = m_policy->get(7);
    EXPECT_EQ(pnode2, pnode);
}

} // namespace test
