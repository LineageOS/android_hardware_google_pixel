/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include <battery_mitigation/BatteryMitigationService.h>

namespace android {
namespace hardware {
namespace google {
namespace pixel {

using android::base::ReadFileToString;
using android::base::StartsWith;

BatteryMitigationService::BatteryMitigationService(
                          const struct MitigationConfig::EventThreadConfig &eventThreadCfg,
                          int platformNum)
                          :cfg(eventThreadCfg), platformNum(platformNum) {
    initTotalNumericSysfsPaths();
    platformIdx = platformNum - MIN_SUPPORTED_PLATFORM;
}

BatteryMitigationService::~BatteryMitigationService() {
    stopBrownoutEventThread();
}

bool BatteryMitigationService::isBrownoutStatsBinarySupported() {
    if (access(cfg.TriggeredIdxPath, F_OK) == 0 &&
        access(cfg.BrownoutStatsPath, F_OK) == 0) {
        return true;
    }
    return false;
}

bool BatteryMitigationService::isPlatformSupported() {
    if (platformNum >= MIN_SUPPORTED_PLATFORM &&
        platformNum <= MAX_SUPPORTED_PLATFORM) {
        return true;
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

bool readSysfsToDouble(const std::string &path, double *val) {
    std::string file_contents;

    if (!android::base::ReadFileToString(path, &file_contents)) {
        return false;
    } else if (sscanf(file_contents.c_str(), "%lf", val) != 1) {
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

void BatteryMitigationService::initTotalNumericSysfsPaths() {
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

int BatteryMitigationService::readNumericStats(struct BrownoutStatsExtend *brownoutStatsExtend) {
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


void BatteryMitigationService::startBrownoutEventThread() {
    if (isPlatformSupported() && isBrownoutStatsBinarySupported()) {
        brownoutEventThread = std::thread(&BatteryMitigationService::BrownoutEventThread, this);
    }
}

void BatteryMitigationService::stopBrownoutEventThread() {
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

void BatteryMitigationService::tearDownBrownoutEventThread() {
    close(triggeredIdxFd);
    close(brownoutStatsFd);
    close(triggeredIdxEpollFd);
    close(wakeupEventFd);
    if (storingAddr != nullptr) {
        munmap(storingAddr, sizeof(struct BrownoutStatsExtend) * DUMP_TIMES * \
               BROWNOUT_EVENT_BUF_SIZE);
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

int BatteryMitigationService::initThisMeal() {
    int ret;
    int fd;
    size_t memSize = sizeof(struct BrownoutStatsExtend) * DUMP_TIMES * \
                            BROWNOUT_EVENT_BUF_SIZE;

    ret = getMmapAddr(fd, cfg.StoringPath, memSize, &storingAddr);
    if (ret != 0) {
        LOG(DEBUG) << "can't generate " << cfg.StoringPath;
        return ret;
    }
    memset(storingAddr, 0, memSize);

    return 0;
}

int BatteryMitigationService::initFd() {
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

void BatteryMitigationService::BrownoutEventThread() {
    int requestedFd;
    int triggeredIdx;
    char buf[BUF_SIZE];
    struct timeval eventReceivedTime;
    struct timeval statStoredTime;
    struct epoll_event events[EPOLL_MAXEVENTS];
    struct BrownoutStatsExtend *brownoutStatsExtendHead;
    size_t brownoutStatsSize = sizeof(struct brownout_stats);
    bool stopByEvent = false;
    /* BrownoutEventThread store multiple brownout event (BROWNOUT_EVENT_BUF_SIZE)
     * and each event contains several dumps (DUMP_TIMES).
     */
    int brownoutEventCounter = 0;

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
        struct BrownoutStatsExtend *brownoutStatsExtendEventHead = brownoutStatsExtendHead + \
                                                                   (brownoutEventCounter * DUMP_TIMES);
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
        gettimeofday(&eventReceivedTime, NULL);
        brownoutStatsExtendEventHead->eventReceivedTime = eventReceivedTime;
        lseek(triggeredIdxFd, 0, SEEK_SET);
        if (read(triggeredIdxFd, buf, BUF_SIZE) == -1) {
            continue;
        }
        triggeredIdx = atoi(buf);
        if (triggeredIdx >= TRIGGERED_SOURCE_MAX || triggeredIdx < 0) {
            continue;
        }
        brownoutStatsExtendEventHead->eventIdx = triggeredIdx;

        /* dump brownout related stats */
        std::string stats;
        for (int i = 0; i < DUMP_TIMES; i++) {
            struct BrownoutStatsExtend *brownoutStatsExtend = brownoutStatsExtendEventHead + i;
            /* storing by string due the stats msg too complicate */
            if (ReadFileToString(cfg.FvpStatsPath, &stats)) {
                snprintf(brownoutStatsExtend->fvpStats, FVP_STATS_SIZE, "%s", stats.c_str());
            }
            if (ReadFileToString(cfg.PlatformSpecific[platformIdx].PcieModemPath, &stats)) {
                snprintf(brownoutStatsExtend->pcieModem, UP_DOWN_LINK_SIZE, "%s", stats.c_str());
            }
            if (ReadFileToString(cfg.PlatformSpecific[platformIdx].PcieWifiPath, &stats)) {
                snprintf(brownoutStatsExtend->pcieWifi, UP_DOWN_LINK_SIZE, "%s", stats.c_str());
            }

            /* storing numericStats */
            readNumericStats(brownoutStatsExtend);

            /* storing brownoutStats */
            lseek(brownoutStatsFd, 0, SEEK_SET);
            read(brownoutStatsFd, &brownoutStatsExtend->brownoutStats, brownoutStatsSize);
            gettimeofday(&statStoredTime, NULL);
            brownoutStatsExtend->dumpTime = statStoredTime;
            brownoutStatsExtend->eventReceivedTime = eventReceivedTime;
            brownoutStatsExtend->eventIdx = triggeredIdx;
        }

        if (++brownoutEventCounter == BROWNOUT_EVENT_BUF_SIZE) {
            brownoutEventCounter = 0;
        }
    }
}

void readLPFPowerBitResolutions(const char *odpmDir, double *bitResolutions) {
    char path[BUF_SIZE];

    for (int i = 0; i < METER_CHANNEL_MAX; i++) {
        snprintf(path, BUF_SIZE, "%s/in_power%d_scale", odpmDir, i);
        if (!readSysfsToDouble(path, &bitResolutions[i])) {
            bitResolutions[i] = 0;
        }
    }
}

void readLPFChannelNames(const char *odpmEnabledRailsPath, char **lpfChannelNames) {
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    FILE *fp = fopen(odpmEnabledRailsPath, "r");
    if (!fp)
        return;

    int c = 0;
    while ((read = getline(&line, &len, fp)) != -1 && read != 0) {
        lpfChannelNames[c] = (char *)malloc(read);
        if (lpfChannelNames[c] != nullptr) {
            snprintf(lpfChannelNames[c], read, "%s", line);
        }
        if (++c == METER_CHANNEL_MAX)
            break;
    }
    fclose(fp);

    if (line)
        free(line);
}

int getMainPmicID(const std::string &mainPmicNamePath, const std::string &mainPmicName) {
    std::string content;
    int ret = 0;

    if (!ReadFileToString(mainPmicNamePath, &content)) {
        LOG(DEBUG) << "Failed to open " << mainPmicNamePath << " set device0 as main pmic";
        return ret;
    }

    if (content.compare(mainPmicName) != 0) {
        ret = 1;
    }

    return ret;
}

void freeLpfChannelNames(char **lpfChannelNames) {
    for (int c = 0; c < METER_CHANNEL_MAX; c++){
        free(lpfChannelNames[c]);
    }
}

void printUTC(FILE *fp, struct timespec time, const char *stat) {
    if (!fp) {
        return;
    }
    char timeBuff[BUF_SIZE];
    if (strlen(stat) > 0) {
        fprintf(fp, "%s: ", stat);
    }
    std::strftime(timeBuff, BUF_SIZE, "%Y-%m-%d_%H:%M:%S", std::localtime(&time.tv_sec));
    fprintf(fp, "%s.%09ld",timeBuff, time.tv_nsec);
}

void printUTC(FILE *fp, timeval time, const char *stat) {
    if (!fp) {
        return;
    }
    char timeBuff[BUF_SIZE];
    if (strlen(stat) > 0) {
        fprintf(fp, "%s: ", stat);
    }
    std::strftime(timeBuff, BUF_SIZE, "%Y-%m-%d_%H:%M:%S", std::localtime(&time.tv_sec));
    /* convert usec to nsec */
    fprintf(fp, "%s.%06ld000",timeBuff, time.tv_usec);
}

void printODPMInstantDataSummary(FILE *fp, std::vector<odpm_instant_data> &odpmData,
                                 double *lpfBitResolutions, char **lpfChannelNames) {
    if (!fp) {
        return;
    }
    std::vector<struct timespec> validTime;
    std::vector<OdpmInstantPower> instPower[METER_CHANNEL_MAX];
    std::vector<OdpmInstantPower> instPowerMax;
    std::vector<OdpmInstantPower> instPowerMin;
    std::vector<double> instPowerList;
    std::vector<double> instPowerStd;

    if (odpmData.size() == 0)
        return;

    /* initial Max, Min, Sum for sorting */
    struct timespec curTime = odpmData[0].time;
    validTime.emplace_back(curTime);
    for (int c = 0; c < METER_CHANNEL_MAX; c++) {
        double power = lpfBitResolutions[c] * odpmData[0].value[c];
        instPower[c].emplace_back((OdpmInstantPower){curTime, power});
        instPowerMax.emplace_back((OdpmInstantPower){curTime, power});
        instPowerMin.emplace_back((OdpmInstantPower){curTime, power});
        instPowerList.emplace_back(power);
    }

    for (auto lpf = (odpmData.begin() + 1); lpf != odpmData.end(); lpf++) {
        curTime = lpf->time;
        /* remove duplicate data by checking the odpm instant data dump time */
        auto it =  std::find_if(validTime.begin(), validTime.end(),
                                [&_ts = curTime] (const struct timespec &ts) ->
                                bool {return _ts.tv_sec  == ts.tv_sec && _ts.tv_nsec  == ts.tv_nsec;});
        if (it == validTime.end()) {
            validTime.emplace_back(curTime);
            for (int c = 0; c < METER_CHANNEL_MAX; c++){
                double power = lpfBitResolutions[c] * lpf->value[c];
                instPower[c].emplace_back((OdpmInstantPower){curTime, power});
                instPowerList[c] += power;
                if (power > instPowerMax[c].value) {
                    instPowerMax[c].value = power;
                    instPowerMax[c].time = curTime;
                }
                if (power < instPowerMin[c].value) {
                    instPowerMin[c].value = power;
                    instPowerMin[c].time = curTime;
                }
            }
        }
    }

    int n = validTime.size();
    for (int c = 0; c < METER_CHANNEL_MAX; c++) {
        /* sort instant power by time */
        std::sort(instPower[c].begin(), instPower[c].end(),
                  [] (const auto &i, const auto &j)
                  {return i.time.tv_sec <= j.time.tv_sec && i.time.tv_nsec < j.time.tv_nsec;});
        /* compute std for each channel */
        double avg = instPowerList[c] / n;
        double mse = 0;
        for (int i = 0; i < n; i++) {
            mse += pow(instPower[c][i].value - avg, 2);
        }
        instPowerStd.emplace_back(pow(mse / n, 0.5));
    }

    /* print Max, Min, Avg, Std */
    for (int c = 0; c < METER_CHANNEL_MAX; c++) {
        fprintf(fp, "%s Max: %.2f Min: %.2f Avg: %.2f Std: %.2f\n", lpfChannelNames[c],
               instPowerMax[c].value,
               instPowerMin[c].value,
               instPowerList[c] / n,
               instPowerStd[c]);
    }
    fprintf(fp, "\n");

    /* print time */
    fprintf(fp, "time ");
    for (int i = 0; i < n; i++) {
        printUTC(fp, instPower[0][i].time, "");
        fprintf(fp, " ");
    }
    fprintf(fp, "\n");

    /* print instant power by channel */
    for (int c = 0; c < METER_CHANNEL_MAX; c++){
        fprintf(fp, "%s ", lpfChannelNames[c]);
        for (int i = 0; i < n; i++) {
            fprintf(fp, "%.2f ", instPower[c][i].value);
        }
        fprintf(fp, "\n");
    }

    fprintf(fp, "\n");
}

void printLatency(FILE *fp, struct BrownoutStatsExtend *brownoutStatsExtend) {
    if (!fp) {
        return;
    }
    /* received latency */
    struct timespec recvLatency;
    recvLatency.tv_sec = brownoutStatsExtend[0].eventReceivedTime.tv_sec - \
                         brownoutStatsExtend[0].brownoutStats.triggered_time.tv_sec;

    signed long long temp = brownoutStatsExtend[0].eventReceivedTime.tv_usec * 1000;
    if (temp >= brownoutStatsExtend[0].brownoutStats.triggered_time.tv_nsec)
        recvLatency.tv_nsec = brownoutStatsExtend[0].eventReceivedTime.tv_usec * 1000 - \
                              brownoutStatsExtend[0].brownoutStats.triggered_time.tv_nsec;
    else
        recvLatency.tv_nsec = NSEC_PER_SEC - \
                              brownoutStatsExtend[0].brownoutStats.triggered_time.tv_nsec \
                              + brownoutStatsExtend[0].eventReceivedTime.tv_usec * 1000;

    /* dump latency */
    struct timespec dumpLatency;
    dumpLatency.tv_sec = brownoutStatsExtend[0].dumpTime.tv_sec - \
                         brownoutStatsExtend[0].eventReceivedTime.tv_sec;

    temp = brownoutStatsExtend[0].dumpTime.tv_usec;
    if (temp >= brownoutStatsExtend[0].eventReceivedTime.tv_usec)
        dumpLatency.tv_nsec = (brownoutStatsExtend[0].dumpTime.tv_usec - \
                                brownoutStatsExtend[0].eventReceivedTime.tv_usec) * 1000;
    else
        dumpLatency.tv_nsec = NSEC_PER_SEC - \
                              brownoutStatsExtend[0].eventReceivedTime.tv_usec * 1000 +	\
                              brownoutStatsExtend[0].dumpTime.tv_usec * 1000;

    /* total latency */
    struct timespec totalLatency;
    totalLatency.tv_sec = brownoutStatsExtend[0].dumpTime.tv_sec - \
                          brownoutStatsExtend[0].brownoutStats.triggered_time.tv_sec;
    temp = brownoutStatsExtend[0].dumpTime.tv_usec * 1000;
    if (temp >= brownoutStatsExtend[0].brownoutStats.triggered_time.tv_nsec)
        totalLatency.tv_nsec = brownoutStatsExtend[0].dumpTime.tv_usec * 1000 - \
            brownoutStatsExtend[0].brownoutStats.triggered_time.tv_nsec;
    else
        totalLatency.tv_nsec = NSEC_PER_SEC - \
                               brownoutStatsExtend[0].brownoutStats.triggered_time.tv_nsec + \
                               brownoutStatsExtend[0].dumpTime.tv_usec * 1000;

    fprintf(fp, "recvLatency %ld.%09ld\n", recvLatency.tv_sec, recvLatency.tv_nsec);
    fprintf(fp, "dumpLatency %ld.%09ld\n", dumpLatency.tv_sec, dumpLatency.tv_nsec);
    fprintf(fp, "totalLatency %ld.%09ld\n\n", totalLatency.tv_sec, totalLatency.tv_nsec);

}

void BatteryMitigationService::printBrownoutStatsExtendSummary(
                               FILE *fp, struct BrownoutStatsExtend *brownoutStatsExtend) {
    if (!fp) {
        return;
    }
    int mainPmicID = 0;
    mainPmicID = getMainPmicID(cfg.PmicCommon[mainPmicID].PmicNamePath, cfg.PlatformSpecific[platformIdx].MainPmicName);
    int subPmicID = !mainPmicID;
    double mainLpfBitResolutions[METER_CHANNEL_MAX];
    double subLpfBitResolutions[METER_CHANNEL_MAX];
    char *mainLpfChannelNames[METER_CHANNEL_MAX];
    char *subLpfChannelNames[METER_CHANNEL_MAX];
    std::vector<odpm_instant_data> odpmData[PMIC_NUM];

    /* print out the triggered_time in first dump */
    printUTC(fp, brownoutStatsExtend[0].brownoutStats.triggered_time, "triggered_time");
    fprintf(fp, "\n");
    fprintf(fp, "triggered_idx: %d\n", brownoutStatsExtend[0].brownoutStats.triggered_idx);
    printLatency(fp, brownoutStatsExtend);

    /* skip time invalid odpm instant data */
    for (int i = 0; i < DUMP_TIMES; i++) {
        for (int d = 0; d < DATA_LOGGING_LEN; d++) {
            if (brownoutStatsExtend[i].brownoutStats.main_odpm_instant_data[d].time.tv_sec != 0) {
                odpmData[mainPmicID].emplace_back(brownoutStatsExtend[i].brownoutStats.main_odpm_instant_data[d]);
            }
            if (brownoutStatsExtend[i].brownoutStats.sub_odpm_instant_data[d].time.tv_sec != 0) {
                odpmData[subPmicID].emplace_back(brownoutStatsExtend[i].brownoutStats.sub_odpm_instant_data[d]);
            }
        }
    }

    /* read odpm resolutions and channel names */
    readLPFPowerBitResolutions(cfg.PmicCommon[mainPmicID].OdpmDir, mainLpfBitResolutions);
    readLPFPowerBitResolutions(cfg.PmicCommon[subPmicID].OdpmDir, subLpfBitResolutions);
    readLPFChannelNames(cfg.PmicCommon[mainPmicID].OdpmEnabledRailsPath, mainLpfChannelNames);
    readLPFChannelNames(cfg.PmicCommon[subPmicID].OdpmEnabledRailsPath, subLpfChannelNames);

    printODPMInstantDataSummary(fp, odpmData[mainPmicID], mainLpfBitResolutions, mainLpfChannelNames);
    printODPMInstantDataSummary(fp, odpmData[subPmicID], subLpfBitResolutions, subLpfChannelNames);

    freeLpfChannelNames(mainLpfChannelNames);
    freeLpfChannelNames(subLpfChannelNames);
}

void printOdpmInstantData(FILE *fp, struct odpm_instant_data odpmInstantData) {
    if (!fp) {
        return;
    }
    if (odpmInstantData.time.tv_sec == 0 &&
        odpmInstantData.time.tv_nsec == 0) {
        return;
    }
    printUTC(fp, odpmInstantData.time, "");
    fprintf(fp, " ");
    for (int i = 0; i < METER_CHANNEL_MAX; i++){
        fprintf(fp, "%d ", odpmInstantData.value[i]);
    }
    fprintf(fp, "\n");
}

void printBrownoutStatsExtendRaw(FILE *fp, struct BrownoutStatsExtend *brownoutStatsExtend) {
    if (!fp) {
        return;
    }
    printUTC(fp, brownoutStatsExtend->brownoutStats.triggered_time, "triggered_time");
    fprintf(fp, "\n");
    fprintf(fp, "triggered_idx: %d\n", brownoutStatsExtend->brownoutStats.triggered_idx);

    fprintf(fp, "main_odpm_instant_data: \n");
    for (int d = 0; d < DATA_LOGGING_LEN; d++) {
        printOdpmInstantData(fp, brownoutStatsExtend->brownoutStats.main_odpm_instant_data[d]);
    }
    fprintf(fp, "sub_odpm_instant_data: \n");
    for (int d = 0; d < DATA_LOGGING_LEN; d++) {
        printOdpmInstantData(fp, brownoutStatsExtend->brownoutStats.sub_odpm_instant_data[d]);
    }

    fprintf(fp, "\n");
    fprintf(fp, "fvp_stats:\n");
    /* show cur_freq only */
    std::string fvpStats(brownoutStatsExtend->fvpStats);
    std::string line;
    std::istringstream iss(fvpStats);
    while (getline(iss, line, '\n')) {
        if (line.find("time_ns") == std::string::npos) {
            fprintf(fp, "%s\n", line.c_str());
        }
    }
    fprintf(fp, "\n");
    fprintf(fp, "pcie_modem:\n");
    fprintf(fp, "%s\n\n", brownoutStatsExtend->pcieModem);
    fprintf(fp, "pcie_wifi:\n");
    fprintf(fp, "%s\n\n", brownoutStatsExtend->pcieWifi);
    for (int i = 0; i < STATS_MAX_SIZE; i++) {
        if (strlen(brownoutStatsExtend->numericStats[i].name) > 0)
            fprintf(fp, "%s: %d\n", brownoutStatsExtend->numericStats[i].name,
                               brownoutStatsExtend->numericStats[i].value);
    }
    printUTC(fp, brownoutStatsExtend->eventReceivedTime, "eventReceivedTime");
    fprintf(fp, "\n");
    printUTC(fp, brownoutStatsExtend->dumpTime, "dumpTime");
    fprintf(fp, "\n");
    fprintf(fp, "eventIdx: %d\n", brownoutStatsExtend->eventIdx);
}

bool readBrownoutStatsExtend(const char *storingPath,
                             struct BrownoutStatsExtend *brownoutStatsExtends) {
    int fd = open(storingPath, O_RDONLY);
    if (fd < 0) {
        LOG(DEBUG) << "Failed to open %s " << storingPath;
        return false;
    }

    size_t memSize = sizeof(struct BrownoutStatsExtend) * DUMP_TIMES * BROWNOUT_EVENT_BUF_SIZE;
    size_t logFileSize = lseek(fd, 0, SEEK_END);
    if (memSize != logFileSize) {
        LOG(DEBUG) << storingPath << " size not match";
        close(fd);
        return false;
    }

    char *logFileAddr = (char *) mmap(NULL, logFileSize, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    memcpy(brownoutStatsExtends, logFileAddr, logFileSize);
    munmap(logFileAddr, logFileSize);

    return true;
}

bool BatteryMitigationService::isTimeValid(const char *storingPath,
                                           std::chrono::system_clock::time_point startTime) {
    struct BrownoutStatsExtend brownoutStatsExtends[BROWNOUT_EVENT_BUF_SIZE][DUMP_TIMES];
    if (!readBrownoutStatsExtend(storingPath, brownoutStatsExtends[0])) {
        return false;
    }
    time_t sec = std::chrono::time_point_cast<std::chrono::seconds>(startTime).time_since_epoch().count();

    for (int e = 0; e < BROWNOUT_EVENT_BUF_SIZE; e++) {
        if (brownoutStatsExtends[e][0].dumpTime.tv_sec == 0 &&
            brownoutStatsExtends[e][0].dumpTime.tv_usec == 0) {
            continue;
        } else if (brownoutStatsExtends[e][0].dumpTime.tv_sec < sec) {
            return true;
        }
    }

    return false;
}

bool BatteryMitigationService::parseBrownoutStatsExtend(FILE *fp) {
    if (!fp) {
        return false;
    }
    struct BrownoutStatsExtend brownoutStatsExtends[BROWNOUT_EVENT_BUF_SIZE][DUMP_TIMES];
    if (!readBrownoutStatsExtend(cfg.StoringPath, brownoutStatsExtends[0])) {
        return false;
    }

    for (int e = 0; e < BROWNOUT_EVENT_BUF_SIZE; e++) {
        if (brownoutStatsExtends[e][0].dumpTime.tv_sec == 0 &&
            brownoutStatsExtends[e][0].dumpTime.tv_usec == 0) {
            continue;
        }
        printBrownoutStatsExtendSummary(fp, brownoutStatsExtends[e]);
        fprintf(fp, "=== RAW ===\n");
        for (int i = 0; i < DUMP_TIMES; i++) {
            fprintf(fp, "=== Dump %d-%d ===\n", e, i);
            printBrownoutStatsExtendRaw(fp, &brownoutStatsExtends[e][i]);
            fprintf(fp, "=============\n\n");
        }
    }
    return true;
}

bool BatteryMitigationService::genParsedMeal(const char *parsedMealPath) {
    FILE *fp = nullptr;

    if (access(cfg.StoringPath, F_OK) != 0) {
        LOG(DEBUG) << "Failed to access " << cfg.StoringPath;
        return false;
    } else {
        fp = fopen(parsedMealPath, "w");
        if (!fp || !parseBrownoutStatsExtend(fp)) {
            return false;
        }
        fclose(fp);
    }
    return true;
}

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
