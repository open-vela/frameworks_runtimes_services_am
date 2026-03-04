#
# Copyright (C) 2024 Xiaomi Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#

if(CONFIG_SYSTEM_ACTIVITY_SERVICE)
  set(CURRENT_DIR ${CMAKE_CURRENT_LIST_DIR})
  set(CUR_TARGET activity_manager)

  nuttx_add_prebuilt(BIN_TARGETS activity_manager APP_TARGETS am amTest)

  nuttx_library_ifndef(SKIP_PREBUILT ${CUR_TARGET})
  nuttx_add_dependencies(TARGET ${CUR_TARGET} DEPENDS android_binder
                         package_manager window_manager)

  # complie options
  set(CFLAGS -Wno-class-memaccess -Wno-array-bounds)
  set(INCDIR ${CURRENT_DIR}/include ${CURRENT_DIR}/../include
             ${CMAKE_CURRENT_BINARY_DIR}/aidl)
  file(GLOB_RECURSE CXXRCS ${CURRENT_DIR}/app/*.cpp ${CURRENT_DIR}/server/*.cpp)

  nuttx_compile_options(${CFLAGS})
  nuttx_sources(${CXXRCS})
  nuttx_include_directories(${INCDIR})

  # export header file
  set_property(
    TARGET nuttx
    APPEND
    PROPERTY NUTTX_CXX_INCLUDE_DIRECTORIES ${CURRENT_DIR}/include
             ${CMAKE_CURRENT_BINARY_DIR}/aidl)

  # aidl sources
  file(GLOB_RECURSE AIDLS aidl/*.aidl)
  nuttx_add_aidl(
    TARGET
    ${CUR_TARGET}
    AIDL_BASE_DIR
    ${CURRENT_DIR}/aidl
    AIDL_INCLUDE_DIR
    ${CURRENT_DIR}/aidl
    AIDL_HEADER_DIR
    ${CMAKE_CURRENT_BINARY_DIR}/aidl
    AIDL_OUT_DIR
    ${CMAKE_CURRENT_BINARY_DIR}/aidl
    AIDL_FLAGS
    --lang=cpp
    AIDLS
    ${AIDLS})

  # am command tool
  if(CONFIG_AM_COMMAND)
    nuttx_add_application(
      NAME
      am
      STACKSIZE
      ${CONFIG_DEFAULT_TASK_STACKSIZE}
      PRIORITY
      SCHED_PRIORITY_DEFAULT
      SRCS
      cmd/AmCommand.cpp
      INCLUDE_DIRECTORIES
      ${INCDIR}
      DEPENDS
      ${CUR_TARGET})
  endif()

  # UvLoop test
  if(CONFIG_AM_TEST)
    nuttx_add_application(
      NAME
      amTest
      STACKSIZE
      ${CONFIG_DEFAULT_TASK_STACKSIZE}
      PRIORITY
      SCHED_PRIORITY_DEFAULT
      SRCS
      test/am_test_main.cpp
      test/UvLoopTest.cpp
      test/IntentTest.cpp
      test/ProcessPriorityPolicyTest.cpp
      INCLUDE_DIRECTORIES
      ${INCDIR}
      DEPENDS
      ${CUR_TARGET}
      googletest)
  endif()

endif()
