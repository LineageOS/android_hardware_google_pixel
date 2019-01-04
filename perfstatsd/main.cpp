/*
 * Copyright (C) 2018 The Android Open Source Project
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

#define LOG_TAG "perfstatsd"

#include <perfstatsd.h>
#include <perfstatsd_service.h>

using namespace android;
using namespace android::base;
using namespace std;
using namespace std::this_thread;

enum MODE { DUMP_HISTORY, SET_OPTION };

sp<perfstatsd_t> perfstatsd_sp;

void *perfstatsd_main(void *) {
    LOG_TO(SYSTEM, INFO) << "main thread started";
    perfstatsd_sp = new perfstatsd_t();

    while (true) {
        perfstatsd_sp->refresh();
        perfstatsd_sp->pause();
    }
    return NULL;
}

void help(char **argv) {
    string usage = argv[0];
    usage = "Usage: " + usage + " [-s][-d][-o]\n" +
            "Options:\n"
            "    -s, start as service\n"
            "    -d, dump perf stats history for dumpstate_board\n"
            "    -o, set key/value option";

    fprintf(stderr, "%s\n", usage.c_str());
}

int startService(void) {
    pthread_t perfstatsd_main_thread;
    errno = pthread_create(&perfstatsd_main_thread, NULL, perfstatsd_main, NULL);
    if (errno != 0) {
        PLOG_TO(SYSTEM, ERROR) << "Failed to create main thread";
        return -1;
    } else {
        pthread_setname_np(perfstatsd_main_thread, "perfstatsd_main");
    }

    android::ProcessState::initWithDriver("/dev/vndbinder");

    if (PerfstatsdPrivateService::start() != android::OK) {
        PLOG_TO(SYSTEM, ERROR) << "Failed to start perfstatsd service";
        return -1;
    } else
        LOG_TO(SYSTEM, INFO) << "perfstatsd_pri_service started";

    android::ProcessState::self()->startThreadPool();
    IPCThreadState::self()->joinThreadPool();
    pthread_join(perfstatsd_main_thread, NULL);
    return 0;
}

int serviceCall(int mode, const string &key, const string &value) {
    android::ProcessState::initWithDriver("/dev/vndbinder");

    sp<IPerfstatsdPrivate> perfstatsd_pri_service = get_perfstatsd_pri_service();
    if (perfstatsd_pri_service == NULL) {
        PLOG_TO(SYSTEM, ERROR) << "Cannot find perfstatsd service.";
        fprintf(stdout, "Cannot find perfstatsd service.\n");
        return -1;
    }

    switch (mode) {
        case DUMP_HISTORY: {
            string history;
            LOG_TO(SYSTEM, INFO) << "dump perfstats history.";
            if (!perfstatsd_pri_service->dumpHistory(&history).isOk() || history.empty()) {
                PLOG_TO(SYSTEM, ERROR) << "perf stats history is not available";
                fprintf(stdout, "perf stats history is not available\n");
                return -1;
            }
            fprintf(stdout, "%s\n", history.c_str());
            break;
        }
        case SET_OPTION:
            LOG_TO(SYSTEM, INFO) << "set option: " << key << " , " << value;
            if (!perfstatsd_pri_service
                     ->setOptions(std::forward<const string>(key),
                                  std::forward<const string>(value))
                     .isOk()) {
                PLOG_TO(SYSTEM, ERROR) << "fail to set options";
                fprintf(stdout, "fail to set options\n");
                return -1;
            }
            break;
    }
    return 0;
}

int serviceCall(int mode) {
    string empty("");
    return serviceCall(mode, empty, empty);
}

int main(int argc, char **argv) {
    int c;
    while ((c = getopt(argc, argv, "sdo:h")) != -1) {
        switch (c) {
            case 's':
                return startService();
            case 'd':
                return serviceCall(DUMP_HISTORY);
            case 'o':
                // set options
                if (argc == 4) {
                    string key(argv[2]);
                    string value(argv[3]);
                    return serviceCall(SET_OPTION, std::move(key), std::move(value));
                }
                FALLTHROUGH_INTENDED;
            case 'h':
                // print usage
                FALLTHROUGH_INTENDED;
            default:
                help(argv);
                return 2;
        }
    }
    return 0;
}
