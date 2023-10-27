/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include <battery_mitigation/BatteryMitigation.h>

#include <sstream>

#define ONE_SECOND_IN_US 1000000

namespace android {
namespace hardware {
namespace google {
namespace pixel {

using android::base::ReadFileToString;
using android::base::StartsWith;

BatteryMitigation::BatteryMitigation(const struct MitigationConfig::Config &cfg,
                                     const struct MitigationConfig::EventThreadConfig &eventThreadCfg)
                                     :cfg(eventThreadCfg) {
    mThermalMgr = &MitigationThermalManager::getInstance();
    mThermalMgr->updateConfig(cfg);
    initTotalNumericSysfsPaths();
}

BatteryMitigation::~BatteryMitigation() {
    stopBrownoutEventThread();
}

bool BatteryMitigation::isMitigationLogTimeValid(std::chrono::system_clock::time_point startTime,
                                                 const char *const logFilePath,
                                                 const char *const timestampFormat,
                                                 const std::regex pattern) {
    std::string logFile;
    if (!ReadFileToString(logFilePath, &logFile)) {
        return false;
    }
    std::istringstream content(logFile);
    std::string line;
    int counter = 0;
    std::smatch pattern_match;
    while (std::getline(content, line)) {
        if (std::regex_match(line, pattern_match, pattern)) {
            std::tm triggeredTimestamp = {};
            std::istringstream ss(pattern_match.str());
            ss >> std::get_time(&triggeredTimestamp, timestampFormat);
            auto logFileTime = std::chrono::system_clock::from_time_t(mktime(&triggeredTimestamp));
            auto epoch_logFileTime = logFileTime.time_since_epoch().count() / ONE_SECOND_IN_US;

            // Convert start time to same format
            auto time_sec = std::chrono::system_clock::to_time_t(startTime);
            struct tm start_tm;
            std::stringstream oss;
            localtime_r(&time_sec, &start_tm);
            oss << std::put_time(&start_tm, timestampFormat) << std::flush;
            std::tm startTimestamp = {};
            std::istringstream st(oss.str());
            st >> std::get_time(&startTimestamp, timestampFormat);
            auto start = std::chrono::system_clock::from_time_t(mktime(&startTimestamp));
            auto epoch_startTime = start.time_since_epoch().count() / ONE_SECOND_IN_US;

            auto delta = epoch_startTime - epoch_logFileTime;
            auto delta_minutes = delta / 60;

            if (delta_minutes >= 0) {
                return true;
            }
        }
        counter += 1;
        if (counter > 5) {
            break;
        }
    }
    return false;
}

bool readSysfsToInt(const std::string &path, int *val) {
    std::string file_contents;

    if (!ReadFileToString(path, &file_contents)) {
        return false;
    } else if (StartsWith(file_contents, "0x")) {
        if (sscanf(file_contents.c_str(), "0x%x", val) != 1) {
            return false;
        }
    } else if (sscanf(file_contents.c_str(), "%d", val) != 1) {
        return false;
    }
    return true;
}

int getFilesInDir(const char *directory, std::vector<std::string> *files) {
    std::string content;
    struct dirent *entry;

    DIR *dir = opendir(directory);
    if (dir == NULL)
        return -1;

    files->clear();
    while ((entry = readdir(dir)) != NULL)
        files->push_back(entry->d_name);
    closedir(dir);

    return 0;
}

void BatteryMitigation::initTotalNumericSysfsPaths() {
    std::vector<std::string> files;

    totalNumericSysfsStatPaths.assign(cfg.NumericSysfsStatPaths.begin(),
                                      cfg.NumericSysfsStatPaths.end());

    for (const auto &sysfsStat : cfg.NumericSysfsStatDirs) {
        if (getFilesInDir(sysfsStat.path.c_str(), &files) < 0) {
            continue;
        }
        for (auto &file : files) {
            std::string fullPath = sysfsStat.path + file;
            totalNumericSysfsStatPaths.push_back({file, fullPath});
        }
    }
}

int BatteryMitigation::readNumericStats(struct BrownoutStatsExtend *brownoutStatsExtend) {
    int i = 0;

    if (i >= STATS_MAX_SIZE)
        return 0;

    for (const auto &sysfsStat : totalNumericSysfsStatPaths) {
        snprintf(brownoutStatsExtend->numericStats[i].name,
                 STAT_NAME_SIZE, "%s", sysfsStat.name.c_str());
        if (!readSysfsToInt(sysfsStat.path,
            &brownoutStatsExtend->numericStats[i].value)) {
            continue;
        }
        if (++i == STATS_MAX_SIZE) {
            LOG(DEBUG) << "STATS_MAX_SIZE not enough for NumericStats";
            break;
        }
    }

    return i;
}


void BatteryMitigation::startBrownoutEventThread() {
    brownoutEventThread = std::thread(&BatteryMitigation::BrownoutEventThread, this);
}

void BatteryMitigation::stopBrownoutEventThread() {
    if (!threadStop.load()) {
        threadStop.store(true);
        uint64_t flag = 1;
        /* wakeup epoll_wait */
        write(wakeupEventFd, &flag, sizeof(flag));

        if (brownoutEventThread.joinable()) {
            brownoutEventThread.join();
        }

        tearDownBrownoutEventThread();
    }
}

void BatteryMitigation::tearDownBrownoutEventThread() {
    close(triggeredIdxFd);
    close(brownoutStatsFd);
    close(triggeredIdxEpollFd);
    close(wakeupEventFd);
    if (storingAddr != nullptr) {
        munmap(storingAddr, sizeof(struct BrownoutStatsExtend) * DUMP_TIMES);
    }
    threadStop.store(true);
}

int getMmapAddr(int &fd, const char *const path, size_t memSize, char **addr) {
    fd = open(path, O_RDWR | O_CREAT, (mode_t) 0644);
    if (fd < 0) {
        return fd;
    }
    lseek(fd, memSize - 1, SEEK_SET);
    write(fd,  "", 1);
    *addr = (char *) mmap(NULL, memSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    return 0;
}

int BatteryMitigation::initThisMeal() {
    int ret;
    int fd;
    char *backupAddr = nullptr;
    bool isThismealExist = false;
    size_t memSize = sizeof(struct BrownoutStatsExtend) * DUMP_TIMES;

    if (access(cfg.StoringPath, F_OK) == 0) {
        isThismealExist = true;
    }

    ret = getMmapAddr(fd, cfg.StoringPath, memSize, &storingAddr);
    if (ret != 0) {
        LOG(DEBUG) << "can't generate " << cfg.StoringPath;
        return ret;
    }

    if (isThismealExist) {
        ret = getMmapAddr(fd, cfg.BackupPath, memSize, &backupAddr);
        if (ret != 0) {
            LOG(DEBUG) << "can't generate " << cfg.BackupPath;
            return ret;
        }
        memcpy(backupAddr, storingAddr, memSize);
        munmap(backupAddr, memSize);
    }

    return 0;
}

int BatteryMitigation::initFd() {
    int ret;
    struct epoll_event triggeredIdxEvent;
    struct epoll_event wakeupEvent;

    brownoutStatsFd = open(cfg.BrownoutStatsPath, O_RDONLY);
    if (brownoutStatsFd < 0) {
        return brownoutStatsFd;
    }

    triggeredIdxFd = open(cfg.TriggeredIdxPath, O_RDONLY);
    if (triggeredIdxFd < 0) {
        return triggeredIdxFd;
    }

    triggeredIdxEpollFd = epoll_create(2);
    if (triggeredIdxEpollFd < 0) {
        return triggeredIdxEpollFd;
    }

    wakeupEventFd = eventfd(0, 0);
    if (wakeupEventFd < 0) {
        return wakeupEventFd;
    }

    triggeredIdxEvent = epoll_event();
    triggeredIdxEvent.data.fd = triggeredIdxFd;
    triggeredIdxEvent.events = EPOLLERR | EPOLLWAKEUP;
    ret = epoll_ctl(triggeredIdxEpollFd, EPOLL_CTL_ADD, triggeredIdxFd, &triggeredIdxEvent);
    if (ret < 0) {
        return ret;
    }
    wakeupEvent = epoll_event();
    wakeupEvent.data.fd = wakeupEventFd;
    wakeupEvent.events = EPOLLIN | EPOLLWAKEUP;
    ret = epoll_ctl(triggeredIdxEpollFd, EPOLL_CTL_ADD, wakeupEventFd, &wakeupEvent);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

void BatteryMitigation::BrownoutEventThread() {
    int requestedFd;
    int triggeredIdx;
    char buf[BUF_SIZE];
    struct timeval statStoredTime;
    struct epoll_event events[EPOLL_MAXEVENTS];
    struct BrownoutStatsExtend *brownoutStatsExtendHead;
    size_t brownoutStatsSize = sizeof(struct brownout_stats);
    bool stopByEvent = false;

    /* initThisMeal() will generate lastmeal.bin if thismeal.bin exist */
    if (initThisMeal() != 0) {
        LOG(DEBUG) << "failed to init thismeal.bin";
        tearDownBrownoutEventThread();
        return;
    }
    if (initFd() != 0) {
        LOG(DEBUG) << "failed to init FD";
        tearDownBrownoutEventThread();
        return;
    }
    brownoutStatsExtendHead = (struct BrownoutStatsExtend *)storingAddr;

    /* allow epoll_wait sleep in the first loop */
    read(triggeredIdxFd, buf, BUF_SIZE);

    while (!threadStop.load()) {
        requestedFd = epoll_wait(triggeredIdxEpollFd, events, EPOLL_MAXEVENTS, -1);
        if (requestedFd <= 0) {
            /* ensure epoll_wait can sleep in the next loop */
            read(triggeredIdxFd, buf, BUF_SIZE);
            continue;
        }
        /* triggeredIdxFd: brownout event from kernel */
        /* wakeupEventFd: wakeup epoll_wait to stop thread properly */
        for (int i = 0; i < requestedFd; i++) {
            if (events[i].data.fd == triggeredIdxFd) {
                break;
            } else {
                stopByEvent = true;
            }
        }
        if (stopByEvent) {
            break;
        }

        /* record brownout event idx and received time */
        gettimeofday(&statStoredTime, NULL);
        brownoutStatsExtendHead->eventReceivedTime = statStoredTime;
        lseek(triggeredIdxFd, 0, SEEK_SET);
        if (read(triggeredIdxFd, buf, BUF_SIZE) == -1) {
            continue;
        }
        triggeredIdx = atoi(buf);
        if (triggeredIdx >= TRIGGERED_SOURCE_MAX || triggeredIdx < 0) {
            continue;
        }
        brownoutStatsExtendHead->eventIdx = triggeredIdx;

        /* dump brownout related stats */
        std::string stats;
        for (int i = 0; i < DUMP_TIMES; i++) {
            struct BrownoutStatsExtend *brownoutStatsExtend = brownoutStatsExtendHead + i;
            /* storing by string due the stats msg too complicate */
            if (ReadFileToString(cfg.FvpStatsPath, &stats)) {
                snprintf(brownoutStatsExtend->fvpStats, FVP_STATS_SIZE, "%s", stats.c_str());
            }
            if (ReadFileToString(cfg.PcieModemPath, &stats)) {
                snprintf(brownoutStatsExtend->pcieModem, UP_DOWN_LINK_SIZE, "%s", stats.c_str());
            }
            if (ReadFileToString(cfg.PcieWifiPath, &stats)) {
                snprintf(brownoutStatsExtend->pcieWifi, UP_DOWN_LINK_SIZE, "%s", stats.c_str());
            }

            /* storing numericStats */
            readNumericStats(brownoutStatsExtend);

            /* storing brownoutStats */
            lseek(brownoutStatsFd, 0, SEEK_SET);
            read(brownoutStatsFd, &brownoutStatsExtend->brownoutStats, brownoutStatsSize);
            gettimeofday(&statStoredTime, NULL);
            brownoutStatsExtend->dumpTime = statStoredTime;
        }
    }
}

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
