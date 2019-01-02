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

#define LOG_TAG "perfstatsd_cpu"

#include "cpu_usage.h"
#include <android-base/stringprintf.h>
#include <android-base/strings.h>

using namespace android::pixel::perfstatsd;

static bool cDebug = false;
static constexpr char FMT_CPU_TOTAL[] =
    "[CPU: %lld.%03llds][T:%.2f%%,U:%.2f%%,S:%.2f%%,IO:%.2f%%]";
static constexpr char TOP_HEADER[] = "[CPU_TOP]  PID, PROCESS NAME, USR_TIME, SYS_TIME\n";
static constexpr char FMT_TOP_PROFILE[] = "%6.2f%%   %5d %s %" PRIu64 " %" PRIu64 "\n";

cpu_usage::cpu_usage(void) {
    std::string procstat;
    if (android::base::ReadFileToString("/proc/stat", &procstat)) {
        std::istringstream stream(procstat);
        std::string line;
        while (getline(stream, line)) {
            std::vector<std::string> fields = android::base::Split(line, " ");
            if (fields[0].find("cpu") != std::string::npos && fields[0] != "cpu") {
                cpudata data;
                mPrevCoresUsage.push_back(data);
            }
        }
    }
    mCores = mPrevCoresUsage.size();
    mProfileThreshold = CPU_USAGE_PROFILE_THRESHOLD;
    mTopcount = TOP_PROCESS_COUNT;
}

void cpu_usage::setOptions(const std::string &key, const std::string &value) {
    if (key == PROCPROF_THRESHOLD || key == CPU_DISABLED || key == CPU_DEBUG ||
        key == CPU_TOPCOUNT) {
        uint32_t val = 0;
        if (!base::ParseUint(value, &val)) {
            LOG_TO(SYSTEM, ERROR) << "Invalid value: " << value;
            return;
        }

        if (key == PROCPROF_THRESHOLD) {
            mProfileThreshold = val;
            LOG_TO(SYSTEM, INFO) << "set profile threshold " << mProfileThreshold;
        } else if (key == CPU_DISABLED) {
            mDisabled = (val != 0);
            LOG_TO(SYSTEM, INFO) << "set disabled " << mDisabled;
        } else if (key == CPU_DEBUG) {
            cDebug = (val != 0);
            LOG_TO(SYSTEM, INFO) << "set debug " << cDebug;
        } else if (key == CPU_TOPCOUNT) {
            mTopcount = val;
            LOG_TO(SYSTEM, INFO) << "set top count " << mTopcount;
        }
    }
}

void cpu_usage::profileProcess(uint64_t diffcpu, std::string *out) {
    // Read cpu usage per process and find the top ones
    DIR *dir;
    struct dirent *ent;
    std::unordered_map<uint32_t, procdata> proc_usage;
    std::priority_queue<procdata, std::vector<procdata>, ProcdataCompare> proclist;
    if ((dir = opendir("/proc/")) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_type == DT_DIR) {
                std::string pid_str = ent->d_name;
                std::string::const_iterator it = pid_str.begin();
                while (it != pid_str.end() && isdigit(*it)) ++it;
                if (!pid_str.empty() && it == pid_str.end()) {
                    std::string pid_stat;
                    if (android::base::ReadFileToString("/proc/" + pid_str + "/stat", &pid_stat)) {
                        std::vector<std::string> fields = android::base::Split(pid_stat, " ");
                        uint32_t pid = 0;
                        uint64_t utime = 0;
                        uint64_t stime = 0;
                        uint64_t cutime = 0;
                        uint64_t cstime = 0;

                        if (!base::ParseUint(fields[0], &pid) ||
                            !base::ParseUint(fields[13], &utime) ||
                            !base::ParseUint(fields[14], &stime) ||
                            !base::ParseUint(fields[15], &cutime) ||
                            !base::ParseUint(fields[16], &cstime)) {
                            LOG_TO(SYSTEM, ERROR) << "Invalid proc data\n" << pid_stat;
                            continue;
                        }
                        std::string proc = fields[1];
                        std::string name =
                            proc.length() > 2 ? proc.substr(1, proc.length() - 2) : "";
                        uint64_t user = utime + cutime;
                        uint64_t system = stime + cstime;
                        uint64_t totalusage = user + system;

                        uint64_t diffuser = user - mPrevProcdata[pid].user;
                        uint64_t diffsystem = system - mPrevProcdata[pid].system;
                        uint64_t diffusage = totalusage - mPrevProcdata[pid].usage;

                        procdata ldata;
                        ldata.user = user;
                        ldata.system = system;
                        ldata.usage = totalusage;
                        proc_usage[pid] = ldata;

                        float usage_ratio = (float)(diffusage * 100.0 / diffcpu);
                        if (cDebug && usage_ratio > 100) {
                            LOG_TO(SYSTEM, INFO) << "pid: " << pid << " , ratio: " << usage_ratio
                                                 << " , prev usage: " << mPrevProcdata[pid].usage
                                                 << " , cur usage: " << totalusage
                                                 << " , total cpu diff: " << diffcpu;
                        }

                        procdata data;
                        data.pid = pid;
                        data.name = name;
                        data.usage_ratio = usage_ratio;
                        data.user = diffuser;
                        data.system = diffsystem;
                        proclist.push(data);
                    }
                }
            }
        }
        mPrevProcdata = std::move(proc_usage);
        uint32_t count = 0;
        out->append(TOP_HEADER);
        while (!proclist.empty() && count++ < mTopcount) {
            procdata data = proclist.top();
            out->append(android::base::StringPrintf(FMT_TOP_PROFILE, data.usage_ratio, data.pid,
                                                    data.name.c_str(), data.user, data.system));
            proclist.pop();
        }
        closedir(dir);
    } else {
        LOG_TO(SYSTEM, ERROR) << "Fail to open /proc/";
    }
}

void cpu_usage::refresh(void) {
    if (mDisabled)
        return;

    std::string out, proc_stat;
    uint64_t diffcpu;
    float totalRatio = 0.0f;
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

    // Get overall cpu usage
    if (android::base::ReadFileToString("/proc/stat", &proc_stat)) {
        std::istringstream stream(proc_stat);
        std::string line;
        while (getline(stream, line)) {
            std::vector<std::string> fields = android::base::Split(line, " ");
            if (fields[0].find("cpu") != std::string::npos) {
                std::string cpu_str = fields[0];
                std::string core =
                    cpu_str.length() > 3 ? cpu_str.substr(3, cpu_str.length() - 3) : "";
                uint64_t user = 0;
                uint64_t nice = 0;
                uint64_t system = 0;
                uint64_t idle = 0;
                uint64_t iowait = 0;
                uint64_t irq = 0;
                uint64_t softirq = 0;
                uint64_t steal = 0;

                // cpu  6013 3243 6311 92390 517 693 319 0 0 0  <-- (fields[1] = "")
                // cpu0 558 139 568 12135 67 121 50 0 0 0
                uint32_t base = core.compare("") ? 1 : 2;

                if (!base::ParseUint(fields[base], &user) ||
                    !base::ParseUint(fields[base + 1], &nice) ||
                    !base::ParseUint(fields[base + 2], &system) ||
                    !base::ParseUint(fields[base + 3], &idle) ||
                    !base::ParseUint(fields[base + 4], &iowait) ||
                    !base::ParseUint(fields[base + 5], &irq) ||
                    !base::ParseUint(fields[base + 6], &softirq) ||
                    !base::ParseUint(fields[base + 7], &steal)) {
                    LOG_TO(SYSTEM, ERROR) << "Invalid /proc/stat data\n" << line;
                    continue;
                }

                uint64_t cputime = user + nice + system + idle + iowait + irq + softirq + steal;
                uint64_t cpuusage = cputime - idle - iowait;
                uint64_t userusage = user + nice;

                if (!core.compare("")) {
                    uint64_t diffusage = cpuusage - mPrevUsage.cpuusage;
                    diffcpu = cputime - mPrevUsage.cputime;
                    uint64_t diffuser = userusage - mPrevUsage.userusage;
                    uint64_t diffsys = system - mPrevUsage.sysusage;
                    uint64_t diffio = iowait - mPrevUsage.iousage;

                    totalRatio = (float)(diffusage * 100.0 / diffcpu);
                    float userRatio = (float)(diffuser * 100.0 / diffcpu);
                    float sysRatio = (float)(diffsys * 100.0 / diffcpu);
                    float ioRatio = (float)(diffio * 100.0 / diffcpu);

                    if (cDebug) {
                        LOG_TO(SYSTEM, INFO)
                            << "prev total: " << mPrevUsage.cpuusage
                            << " , cur total: " << cpuusage << " , diffusage: " << diffusage
                            << " , diffcpu: " << diffcpu << " , ratio: " << totalRatio;
                    }

                    mPrevUsage.cpuusage = cpuusage;
                    mPrevUsage.cputime = cputime;
                    mPrevUsage.userusage = userusage;
                    mPrevUsage.sysusage = system;
                    mPrevUsage.iousage = iowait;

                    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - mLast);
                    out.append(android::base::StringPrintf(FMT_CPU_TOTAL, ms.count() / 1000,
                                                           ms.count() % 1000, totalRatio,
                                                           userRatio, sysRatio, ioRatio));
                } else {
                    // calculate total cpu usage of each core
                    uint32_t c = 0;
                    if (!base::ParseUint(core, &c)) {
                        LOG_TO(SYSTEM, ERROR) << "Invalid core: " << core;
                        continue;
                    }
                    uint64_t diffusage = cpuusage - mPrevCoresUsage[c].cpuusage;
                    float coreTotalRatio = (float)(diffusage * 100.0 / diffcpu);
                    if (cDebug) {
                        LOG_TO(SYSTEM, INFO)
                            << "core " << c << " , prev cpu usage: " << mPrevCoresUsage[c].cpuusage
                            << " , cur cpu usage: " << cpuusage << " , diffusage: " << diffusage
                            << " , difftotalcpu: " << diffcpu << " , ratio: " << coreTotalRatio;
                    }
                    mPrevCoresUsage[c].cpuusage = cpuusage;

                    char buf[64];
                    sprintf(buf, "%.2f%%]", coreTotalRatio);
                    out.append("[" + core + ":" + std::string(buf));
                }
            }
        }
        out.append("\n");
    } else {
        LOG_TO(SYSTEM, ERROR) << "Fail to read /proc/stat";
    }

    if (totalRatio >= mProfileThreshold) {
        if (cDebug)
            LOG_TO(SYSTEM, INFO) << "Total CPU usage over " << mProfileThreshold << "%";
        std::string profile_result;
        profileProcess(diffcpu, &profile_result);
        if (mProfileProcess) {
            // Dump top processes once met threshold continuously at least twice.
            out.append(profile_result);
        } else
            mProfileProcess = true;
    } else
        mProfileProcess = false;

    append(now, out);
    mLast = now;
    if (cDebug) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - now);
        LOG_TO(SYSTEM, INFO) << "Took " << ms.count() << " ms, data bytes: " << out.length();
    }
}
