/*
 * Copyright (C) 2019 The Android Open Source Project
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
#define LOG_TAG "pwrstats_util"

#include <android-base/logging.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <unordered_map>

#include "PowerStatsUtil.h"

namespace {
volatile std::sig_atomic_t gSignalStatus;
}

class Options {
  public:
    bool daemonMode;
    std::string filePath;
};

static void signalHandler(int signal) {
    gSignalStatus = signal;
}

static void printHelp() {
    std::cout << "pwrstats_util: Prints out device power stats in the form of key/value pairs."
              << std::endl
              << "-d </path/to/file> : daemon mode. Spawns a daemon process and prints out"
              << " its <pid>. kill -INT <pid> will trigger a write to specified file." << std::endl;
}

static Options parseArgs(int argc, char** argv) {
    Options opt = {
            .daemonMode = false,
    };

    int c;
    while ((c = getopt(argc, argv, "d:h")) != -1) {
        switch (c) {
            case 'd':
                opt.daemonMode = true;
                opt.filePath = std::string(optarg);
                break;
            case 'h':
                printHelp();
                exit(EXIT_SUCCESS);
            default:
                exit(EXIT_FAILURE);
        }
    }
    return opt;
}

static void snapshot(void) {
    std::unordered_map<std::string, uint64_t> data;
    PowerStatsUtil util;
    int ret = util.getData(data);
    if (ret) {
        exit(EXIT_FAILURE);
    }

    for (auto const& datum : data) {
        std::cout << datum.first << "=" << datum.second << std::endl;
    }

    exit(EXIT_SUCCESS);
}

static void daemon(const std::string& filePath) {
    // Following a subset of steps outlined in http://man7.org/linux/man-pages/man7/daemon.7.html

    // Call fork to create child process
    pid_t pid;
    if ((pid = fork()) < 0) {
        LOG(ERROR) << "can't fork" << std::endl;
        exit(EXIT_FAILURE);
    } else if (pid != 0) {
        std::cout << "pid = " << pid << std::endl;
        exit(EXIT_SUCCESS);
    }
    // Daemon process:

    // Get maximum number of file descriptors
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        LOG(ERROR) << "can't get file limit" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Close all open file descriptors
    if (rl.rlim_max == RLIM_INFINITY) {
        rl.rlim_max = 1024;
    }
    for (int i = 0; i < rl.rlim_max; i++) {
        close(i);
    }

    // Detach from any terminal and create an independent session
    if (setsid() < 0) {
        LOG(ERROR) << "SID creation failed";
        exit(EXIT_FAILURE);
    }

    // connect /dev/null to standard input, output, and error.
    int devnull = open("/dev/null", O_RDWR | O_CLOEXEC);
    dup2(devnull, STDIN_FILENO);
    dup2(devnull, STDOUT_FILENO);
    dup2(devnull, STDERR_FILENO);

    // Reset the umask to 0
    umask(0);

    // Change the current directory to the root
    // directory (/), in order to avoid that the daemon involuntarily
    // blocks mount points from being unmounted
    if (chdir("/") < 0) {
        LOG(ERROR) << "can't change directory to /" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Install a signal handler
    std::signal(SIGINT, signalHandler);

    // get the start_data
    auto start_time = std::chrono::system_clock::now();

    PowerStatsUtil util;
    std::unordered_map<std::string, uint64_t> start_data;
    int ret = util.getData(start_data);
    if (ret) {
        LOG(ERROR) << "failed to get start data";
        exit(EXIT_FAILURE);
    }

    // Wait for INT signal
    while (gSignalStatus != SIGINT) {
        pause();
    }

    // get the end data
    std::unordered_map<std::string, uint64_t> end_data;
    ret = util.getData(end_data);
    if (ret) {
        LOG(ERROR) << "failed to get end data";
        exit(EXIT_FAILURE);
    }
    auto end_time = std::chrono::system_clock::now();

    std::chrono::duration<double> elapsed_seconds = end_time - start_time;

    // Write data to file
    std::ofstream myfile(filePath);
    if (!myfile.is_open()) {
        LOG(ERROR) << "failed to open file";
        exit(EXIT_FAILURE);
    }
    myfile << "elapsed time: " << elapsed_seconds.count() << "s" << std::endl;
    for (auto const& datum : end_data) {
        myfile << datum.first << "=" << datum.second - start_data[datum.first] << std::endl;
    }

    myfile.close();

    exit(EXIT_SUCCESS);
}

void run(const Options& opt) {
    if (opt.daemonMode) {
        daemon(opt.filePath);
    } else {
        snapshot();
    }
}

int main(int argc, char** argv) {
    Options opt = parseArgs(argc, argv);

    run(opt);

    return 0;
}
