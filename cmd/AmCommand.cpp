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

#include "AmCommand.h"

#include <binder/IBinder.h>

#include "app/Intent.h"

namespace os {
namespace app {

using android::String16;
using std::string;
using std::string_view;

AmCommand::AmCommand() {
    mNextArgs = 0;
}

AmCommand::~AmCommand() {}

string_view AmCommand::nextArg() {
    if (mNextArgs < mArgs.size()) {
        return mArgs[mNextArgs++];
    } else {
        return "";
    }
}

int AmCommand::makeIntent(Intent &intent) {
    android::os::PersistableBundle bundle;
    bool hasTarget = false;
    for (auto param = nextArg(); param != ""; param = nextArg()) {
        if (param[0] != '-' && mNextArgs == 2) {
            /** the "TARGET" only appears on the first param after subcommand */
            intent.setTarget(param.data());
            hasTarget = true;
        } else if (param == "-t") {
            intent.setTarget(nextArg().data());
            hasTarget = true;
        } else if (param == "-a") {
            intent.setAction(nextArg().data());
            hasTarget = true;
        } else if (param == "-f") {
            intent.setFlag(std::stoi(string(nextArg().data())));
        } else if (param == "-d") {
            intent.setData(nextArg().data());
        } else if (param == "--ei") {
            const auto key = String16(nextArg().data());
            const auto value = std::stoi(string(nextArg().data()));
            bundle.putInt(key, value);
        } else if (param == "--eu") {
            const auto key = String16(nextArg().data());
            const auto value = std::stof(string(nextArg().data()));
            bundle.putDouble(key, value);
        } else if (param == "--ez") {
            const auto key = String16(nextArg().data());
            const bool value = (nextArg() == "true");
            bundle.putBoolean(key, value);
        } else if (param == "-e" || param == "--es") {
            const auto key = String16(nextArg().data());
            const auto value = String16(nextArg().data());
            bundle.putString(key, value);
        } else {
            printf("unknow options:%s\n", param.data());
            exit(0);
        }
    }
    if (!hasTarget) {
        printf("Necessary parameters are missing:<TARGET> or <ACTION> need to be set\n");
        exit(0);
    }
    intent.setBundle(bundle);
    return 0;
}

int AmCommand::startActivity() {
    Intent intent;
    makeIntent(intent);
    intent.setFlag(intent.mFlag | Intent::FLAG_ACTIVITY_NEW_TASK);
    android::sp<android::IBinder> token = new android::BBinder();
    return mAm.startActivity(token, intent, -1);
}

int AmCommand::stopActivity() {
    Intent intent;
    makeIntent(intent);
    /** It's a little trick to use flag for resultcode */
    return mAm.stopActivity(intent, intent.mFlag);
}

int AmCommand::startService() {
    Intent intent;
    makeIntent(intent);
    return mAm.startService(intent);
}

int AmCommand::stopService() {
    Intent intent;
    makeIntent(intent);
    return mAm.stopService(intent);
}

int AmCommand::postIntent() {
    Intent intent;
    makeIntent(intent);
    return mAm.postIntent(intent);
}

int AmCommand::dump() {
    const android::Vector<android::String16> args;
    if (auto service = mAm.getService()) {
        android::IInterface::asBinder(service)->dump(fileno(stdout), args);
        return 0;
    } else {
        printf("service is not existent, please check \"systemd\" process\n");
        return -1;
    }
}

int AmCommand::run(int argc, char *argv[]) {
    if (argc < 2) {
        return showUsage();
    }

    for (int i = 1; i < argc; ++i) {
        mArgs.emplace_back(argv[i]);
    }

    const auto subCommand = nextArg();
    if ("start" == subCommand) {
        return startActivity();
    }
    if ("stop" == subCommand) {
        return stopActivity();
    }
    if ("startservice" == subCommand) {
        return startService();
    }
    if ("stopservice" == subCommand) {
        return stopService();
    }
    if ("postintent" == subCommand) {
        return postIntent();
    }
    if ("dump" == subCommand) {
        return dump();
    }

    return showUsage();
}

int AmCommand::showUsage() {
    printf("usage: am [subcommand] [options]\n\n");
    printf(" start <INTENT>\t start Activity\n");
    printf(" stop  <INTENT>\t stop  Activity\n");
    printf(" startservice <INTENT>\n");
    printf(" stopservice  <INTENT>\n");
    printf(" postintent   <INTENT>\n");
    printf(" dump  :show all Activity task\n");
    printf("\n You can make <INTENT> like:\n");
    printf("\t-t \t<TARGET> : '-t' is unnecessary when TARGET as the first param\n");
    printf("\t-a \t<ACTION>\n");
    printf("\t-d \t<DATA>\n");
    printf("\t-e|--es \t<EXTRA_KEY> <EXTRA_STRING_VALUE>: eg. --es name XiaoMing\n");
    printf("\t--ei \t<EXTRA_KEY> <EXTRA_INT_VALUE>  : eg. --ei age 24\n");
    printf("\t--eu \t<EXTRA_KEY> <EXTRA_DOUBLE_VALUE>  : eg. --eu height 183.5\n");
    printf("\t--ez \t<EXTRA_KEY> <EXTRA_BOOLEAN_VALUE>  : eg. --ez student true\n");
    printf("\n");
    return 0;
}

extern "C" int main(int argc, char *argv[]) {
    AmCommand cmd;
    const int ret = cmd.run(argc, argv);
    if (ret != 0) {
        printf("Command execution error:%d\n", ret);
    }
    return ret;
}

} // namespace app
} // namespace os
