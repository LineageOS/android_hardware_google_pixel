/*
 * Copyright (C) 2024 The Android Open Source Project
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

#define LOG_TAG "libpixelusb-usbdp"

#include "include/pixelusb/UsbDpUtils.h"

#include <android-base/file.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>
#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <utils/Log.h>

#include <cstring>

using aidl::android::hardware::usb::DisplayPortAltModeStatus;
using android::base::ParseUint;
using android::base::ReadFileToString;
using android::base::Trim;
using android::base::WriteStringToFile;

#define LINK_TRAINING_STATUS_UNKNOWN "0"
#define LINK_TRAINING_STATUS_SUCCESS "1"
#define LINK_TRAINING_STATUS_FAILURE "2"
#define LINK_TRAINING_STATUS_FAILURE_SINK "3"

#define DISPLAYPORT_CAPABILITIES_RECEPTACLE_BIT 6

#define DISPLAYPORT_STATUS_DEBOUNCE_MS 2000
/*
 * Type-C HAL should wait 2 seconds to reattempt DisplayPort Alt Mode entry to
 * allow the port and port partner to settle Role Swaps.
 */
#define DISPLAYPORT_ACTIVATE_DEBOUNCE_MS 2000
// Number of times the HAL should reattempt to enter DisplayPort Alt Mode
#define DISPLAYPORT_ACTIVATE_MAX_RETRIES 2

namespace android {
namespace hardware {
namespace google {
namespace pixel {
namespace usb {
// Set by signal handler to destroy thread
volatile bool destroyDisplayPortThread;

constexpr char kPortPartnerPath[] = "/sys/class/typec/port0-partner/";

UsbDp::UsbDp(const char *const drmPath)
    : mDrmPath(drmPath),
      mPollRunning(false),
      mPollStarting(false),
      mFirstSetupDone(false),
      mIrqCountCache(),
      mCallback(NULL),
      mPayload(NULL),
      mPartnerSupportsDisplayPort(false),
      mLock(PTHREAD_MUTEX_INITIALIZER) {
    pthread_condattr_t attr;
    if (pthread_condattr_init(&attr)) {
        ALOGE("pthread_condattr_init failed: %s", strerror(errno));
        abort();
    }
    if (pthread_condattr_setclock(&attr, CLOCK_MONOTONIC)) {
        ALOGE("pthread_condattr_setclock failed: %s", strerror(errno));
        abort();
    }
    if (pthread_cond_init(&mCV, &attr)) {
        ALOGE("usbdp: pthread_cond_init failed: %s", strerror(errno));
        abort();
    }
    if (pthread_condattr_destroy(&attr)) {
        ALOGE("pthread_condattr_destroy failed: %s", strerror(errno));
        abort();
    }
    mDisplayPortEventPipe = eventfd(0, EFD_NONBLOCK);
    if (mDisplayPortEventPipe == -1) {
        ALOGE("mDisplayPortEventPipe eventfd failed: %s", strerror(errno));
        abort();
    }
    mDisplayPortDebounceTimer = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (mDisplayPortDebounceTimer == -1) {
        ALOGE("mDisplayPortDebounceTimer timerfd failed: %s", strerror(errno));
        abort();
    }
    mActivateTimer = timerfd_create(CLOCK_MONOTONIC, 0);
    if (mActivateTimer == -1) {
        ALOGE("mActivateTimer timerfd failed: %s", strerror(errno));
        abort();
    }
}

/* Class Get/Set Helper Functions */
/**
 * isFirstSetupDone()
 *
 * Return:
 * boolean indicating if first poll thread had been initialized.
 */
bool UsbDp::isFirstSetupDone() {
    return mFirstSetupDone;
}

/**
 * setClientPath()
 *
 * Sets UsbDp.mClientPath.
 *
 * Input:
 * @path: path to I2c/SPMI Client
 */
void UsbDp::setClientPath(string path) {
    mClientPath = path;
}

/**
 * getPollRunning()
 *
 * Return:
 * boolean indicating if mPoll is currently running
 */
bool UsbDp::getPollRunning() {
    return mPollRunning;
}

/**
 * setPartnerSupportsDisplayport()
 *
 * Input:
 * @supportsDp: boolean indicating DP support
 */
void UsbDp::setPartnerSupportsDisplayPort(bool supportsDp) {
    mPartnerSupportsDisplayPort = supportsDp;
}

/**
 * getPartnerSupportsDisplayport()
 *
 * Return:
 * boolean indicating if port partner supports DisplayPort
 */
bool UsbDp::getPartnerSupportsDisplayPort() {
    return mPartnerSupportsDisplayPort;
}

/**
 * updateDisplayPortEventPipe()
 *
 * Writes to mDisplayPortEventPipe
 *
 * Input:
 * @flag: value to write to mDisplayPortEventPipe
 */
void UsbDp::updateDisplayPortEventPipe(uint64_t flag) {
    write(mDisplayPortEventPipe, &flag, sizeof(flag));
}

/**
 * registerCallback()
 *
 * Registers callback to be run when mDisplayPortDebounceTimer triggers
 *
 * Input:
 * @callback: callback function to be performed
 * @payload: pointer to Usb data structure
 */
void UsbDp::registerCallback(void (*callback)(void *(payload)), void *payload) {
    mCallback = callback;
    mPayload = payload;
}

/* Internal fd, epoll, timer Helper Functions */
// Opens file with given flags
static int displayPortPollOpenFileHelper(const char *file, int flags) {
    int fd = open(file, flags);
    if (fd == -1) {
        ALOGE("usbdp: worker: open at %s failed; errno=%d", file, errno);
    }
    return fd;
}

// Sets timerfd (fd) to trigger after (ms) milliseconds.
// Setting ms to 0 disarms the timer.
static int armTimerFdHelper(int fd, int ms) {
    struct itimerspec ts;
    int ret;

    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 0;
    ts.it_value.tv_sec = ms / 1000;
    ts.it_value.tv_nsec = (ms % 1000) * 1000000;

    ret = timerfd_settime(fd, 0, &ts, NULL);
    if (ret < 0) {
        ALOGE("usbdp: %s failed to arm timer", __func__);
    }

    return ret;
}

// Returns timespec that expires in debounceMs ms
static struct timespec setTimespecTimer(int debounceMs) {
    struct timespec to;
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    to.tv_nsec = now.tv_nsec + ((debounceMs % 1000) * 1000000);
    to.tv_sec = now.tv_sec + (debounceMs / 1000);
    // Overflow handling
    to.tv_sec += to.tv_sec / 1000000000L;
    to.tv_nsec = to.tv_nsec % 1000000000L;

    return to;
}

/* Setup/Shutdown Helper Functions */
void *displayPortPollWork(void *param) {
    UsbDp *usbDp = reinterpret_cast<UsbDp *>(param);

    usbDp->displayPortPollWorkHelper();

    return NULL;
}

/**
 * setupDisplayPortPoll()
 *
 * Used by USB HAL to setup DisplayPort work thread. Consecutive calls to
 * setupDisplayPortPoll will exit if the currently starting thread has not
 * established sysfs links, otherwise assume that the file descriptors have
 * become stale and setup needs to be performed again.
 *
 */
void UsbDp::setupDisplayPortPoll() {
    uint64_t flag = DISPLAYPORT_SHUTDOWN_CLEAR;
    mFirstSetupDone = true;
    int ret;

    ALOGI("usbdp: setup: beginning setup for displayport poll thread");
    mPartnerSupportsDisplayPort = true;

    /*
     * If thread is currently starting, then it hasn't setup DisplayPort fd's, and we can abandon
     * this process.
     */
    if (mPollStarting) {
        ALOGI("usbdp: setup: abandoning poll thread because another startup is in progress");
        return;
    }

    /*
     * Check to see if thread is currently running. If it is, then we assume that it must have
     * invalid DisplayPort fd's and the new thread takes over.
     */
    if (mPollRunning) {
        shutdownDisplayPortPoll(true);
        pthread_mutex_lock(&mCVLock);
        struct timespec to = setTimespecTimer(DISPLAYPORT_POLL_WAIT_MS);
        ret = pthread_cond_timedwait(&mCV, &mCVLock, &to);
        if (ret == ETIMEDOUT) {
            ALOGI("usbdp: setup: Wait for poll to shutdown timed out, starting new poll anyways.");
        }
        pthread_mutex_unlock(&mCVLock);
    }

    // Indicate that startup procedure is initiated (mutex protects two threads running setup at
    // once)
    mPollStarting = true;

    // Reset shutdown signals because shutdown() does not perform self clean-up
    write(mDisplayPortEventPipe, &flag, sizeof(flag));
    destroyDisplayPortThread = false;

    /*
     * Create a background thread to poll DisplayPort system files
     */
    if (pthread_create(&mPoll, NULL, displayPortPollWork, this)) {
        ALOGE("usbdp: setup: failed to create displayport poll thread %d", errno);
        goto error;
    }
    ALOGI("usbdp: setup: successfully started displayport poll thread");
    return;

error:
    mPollStarting = false;
    return;
}

/**
 * shutdownDisplayPortPollHelper()
 *
 * Discover the DisplayPort driver sysfs attribute directory. Iterates through
 * all port partner alt mode directories and queries for displayport sysfs group
 * to do so.
 *
 */
void UsbDp::shutdownDisplayPortPollHelper() {
    uint64_t flag = DISPLAYPORT_SHUTDOWN_SET;
    int ret;

    // Write shutdown signal to child thread.
    ret = write(mDisplayPortEventPipe, &flag, sizeof(flag));
    if (ret < 0) {
        ALOGE("usbdp: shutdownDisplayPortPollHelper write failure, %d", ret);
    }
    pthread_join(mPoll, NULL);
    writeHpdOverride(string(mDrmPath), "0");
    pthread_mutex_lock(&mCVLock);
    pthread_cond_signal(&mCV);
    pthread_mutex_unlock(&mCVLock);
}

void *shutdownDisplayPortPollWork(void *param) {
    UsbDp *usbDp = reinterpret_cast<UsbDp *>(param);

    usbDp->shutdownDisplayPortPollHelper();
    ALOGI("usbdp: shutdown: displayport thread shutdown complete.");
    return NULL;
}

/**
 * shutdownDisplayPortPoll()
 *
 * Discover the DisplayPort driver sysfs attribute directory. Iterates through
 * all port partner alt mode directories and queries for displayport sysfs group
 * to do so.
 *
 * Input
 * @force: boolean to indicate if thread should be shutdown irrespective of
 *         whether or not the thread is running.
 *
 */
void UsbDp::shutdownDisplayPortPoll(bool force) {
    string displayPortUsbPath;

    ALOGI("usbdp: shutdown: beginning shutdown for displayport poll thread");

    /*
     * Determine if should shutdown thread
     *
     * getDisplayPortUsbPathHelper locates a DisplayPort directory, no need to double check
     * directory.
     *
     * Force is put in place to shutdown even when displayPortUsbPath is still present.
     * Happens when back to back BIND events are sent and fds are no longer current.
     */
    if (!mPollRunning ||
        (!force && getDisplayPortUsbPathHelper(&displayPortUsbPath) == Status::SUCCESS)) {
        return;
    }
    // Shutdown is nonblocking to let other usb operations continue
    if (pthread_create(&mDisplayPortShutdownHelper, NULL, shutdownDisplayPortPollWork, this)) {
        ALOGE("usbdp: shutdown: shutdown worker pthread creation failed %d", errno);
    }
    ALOGI("usbdp: shutdown: shutdown thread initialized, force:%d", force);
}

/* Sysfs Helper Functions */

/**
 * getDisplayPortUsbPathHelper()
 *
 * Discover the DisplayPort driver sysfs attribute directory. Iterates through
 * all port partner alt mode directories and queries for displayport sysfs group
 * to do so.
 *
 * Input
 * @path: outparameter to save DisplayPort path
 *
 * Return:
 * SUCCESS if sysfs group exists, ERROR otherwise.
 */
Status getDisplayPortUsbPathHelper(string *path) {
    DIR *dp;
    Status result = Status::ERROR;

    dp = opendir(kPortPartnerPath);
    if (dp) {
        struct dirent *ep;
        /* Iterate through all alt modes to find displayport driver */
        while ((ep = readdir(dp))) {
            if (ep->d_type == DT_DIR) {
                DIR *displayPortDp;
                string portPartnerPath =
                        string(kPortPartnerPath) + string(ep->d_name) + "/displayport/";
                displayPortDp = opendir(portPartnerPath.c_str());
                if (displayPortDp) {
                    *path = portPartnerPath;
                    closedir(displayPortDp);
                    result = Status::SUCCESS;
                    break;
                }
            }
        }
        closedir(dp);
    }

    return result;
}

/**
 * readDisplayPortAttribute()
 *
 * Reads value of given sysfs node
 *
 * Input
 * @attribute: sysfs attribute to read. Function supports
 *     "hpd", "pin_assignment", "link_status", and "vdo"
 * @usb_path: path to the port partner's displayport sysfs group
 * @value: outparamenter to write sysfs value to
 *
 * Return:
 * SUCCESS on successful read, ERROR otherwise
 */
Status UsbDp::readDisplayPortAttribute(string attribute, string usb_path, string *value) {
    string attrPath;

    if (!strncmp(attribute.c_str(), "hpd", strlen("hpd")) ||
        !strncmp(attribute.c_str(), "pin_assignment", strlen("pin_assignment"))) {
        attrPath = usb_path + attribute;
    } else if (!strncmp(attribute.c_str(), "link_status", strlen("link_status"))) {
        attrPath = mDrmPath + "link_status";
    } else if (!strncmp(attribute.c_str(), "vdo", strlen("vdo"))) {
        attrPath = usb_path + "/../vdo";
    } else {
        goto error;
    }

    // Read Attribute
    if (ReadFileToString(attrPath.c_str(), value)) {
        return Status::SUCCESS;
    }

error:
    ALOGE("usbdp: Failed to read Type-C attribute %s", attribute.c_str());
    return Status::ERROR;
}

/**
 * writeDisplayPortAttribute()
 *
 * Copy value of usb sysfs attribute to corresponding drm attribute.
 *
 * Input
 * @attribute: sysfs attribute to read. Function supports
 *     "hpd", "irq_hpd_count", "link_status", and "orientation"
 * @usb_path: path to the port partner's displayport sysfs group
 * @drm_path: path to the drm for link_status monitoring
 *
 * Return:
 * SUCCESS on successful write, ERROR otherwise
 */
Status UsbDp::writeDisplayPortAttribute(string attribute, string usb_path) {
    string attrUsb, attrDrm, attrDrmPath;

    // Get Drm Path
    attrDrmPath = mDrmPath + attribute;

    // Read Attribute
    if (!ReadFileToString(usb_path, &attrUsb)) {
        ALOGE("usbdp: Failed to open or read Type-C attribute %s", attribute.c_str());
        return Status::ERROR;
    }
    attrUsb = Trim(attrUsb);

    // Separate Logic for hpd and pin_assignment
    if (!strncmp(attribute.c_str(), "hpd", strlen("hpd"))) {
        if (!strncmp(attrUsb.c_str(), "0", strlen("0"))) {
            // Read DRM attribute to compare
            if (!ReadFileToString(attrDrmPath, &attrDrm)) {
                ALOGE("usbdp: Failed to open or read hpd from drm");
                return Status::ERROR;
            }
            if (!strncmp(attrDrm.c_str(), "0", strlen("0"))) {
                ALOGI("usbdp: Skipping hpd write when drm and usb both equal 0");
                return Status::SUCCESS;
            }
        }
    } else if (!strncmp(attribute.c_str(), "irq_hpd_count", strlen("irq_hpd_count"))) {
        uint32_t temp;
        if (!ParseUint(attrUsb, &temp)) {
            ALOGE("usbdp: failed parsing irq_hpd_count:%s", attrUsb.c_str());
            return Status::ERROR;
        }
        // Used to cache the values read from tcpci's irq_hpd_count.
        // Update drm driver when cached value is not the same as the read value.
        ALOGI("usbdp: mIrqCountCache:%u irq_hpd_count:%u", mIrqCountCache, temp);
        if (mIrqCountCache == temp) {
            return Status::SUCCESS;
        } else {
            mIrqCountCache = temp;
        }
        attrDrmPath = mDrmPath + "irq_hpd";
    } else if (!strncmp(attribute.c_str(), "pin_assignment", strlen("pin_assignment"))) {
        size_t pos = attrUsb.find("[");
        if (pos != string::npos) {
            ALOGI("usbdp: Modifying Pin Config from %s", attrUsb.c_str());
            attrUsb = attrUsb.substr(pos + 1, 1);
        } else {
            // Don't write anything
            ALOGI("usbdp: Pin config not yet chosen, nothing written.");
            return Status::ERROR;
        }
    }

    // Write to drm
    if (!WriteStringToFile(attrUsb, attrDrmPath)) {
        ALOGE("usbdp: Failed to write attribute %s to drm: %s", attribute.c_str(), attrUsb.c_str());
        return Status::ERROR;
    }
    ALOGI("usbdp: Successfully wrote attribute %s: %s to drm.", attribute.c_str(), attrUsb.c_str());
    return Status::SUCCESS;
}

/**
 * writeHpdOverride()
 *
 * Manually writes value to drm hpd stsfs node
 *
 * Input
 * @drm_path: path to the drm
 * @value: hpd value
 *
 * Return:
 * SUCCESS on successful read, ERROR otherwise
 */
Status UsbDp::writeHpdOverride(string drm_path, string value) {
    string attrDrmPath;

    // Get Drm Path
    attrDrmPath = drm_path + "hpd";

    // Write to drm
    if (!WriteStringToFile(value, attrDrmPath)) {
        ALOGE("usbdp: hpd override failed: %s", value.c_str());
        return Status::ERROR;
    }
    ALOGI("usbdp: hpd override success: %s", value.c_str());
    return Status::SUCCESS;
}

/**
 * queryPartnerSvids()
 *
 * query port partner's supported alt mode svids
 *
 * Input
 * @svids: outparameter to return queried svids
 *
 * Return:
 * SUCCESS on successful operation, ERROR otherwise.
 */
Status queryPartnerSvids(std::vector<string> *svids) {
    DIR *dp;

    dp = opendir(kPortPartnerPath);
    if (dp != NULL) {
        struct dirent *ep;
        // Iterate through directories for Alt Mode SVIDs
        while ((ep = readdir(dp))) {
            if (ep->d_type == DT_DIR) {
                string svid;
                string portPartnerPath = string(kPortPartnerPath) + string(ep->d_name) + "/svid";
                if (ReadFileToString(portPartnerPath, &svid)) {
                    (*svids).push_back(Trim(svid));
                }
            }
        }
        closedir(dp);
    } else {
        return Status::ERROR;
    }
    return Status::SUCCESS;
}

/* AIDL Helper Functions */
DisplayPortAltModePinAssignment parsePinAssignmentHelper(string pinAssignments) {
    size_t pos = pinAssignments.find("[");
    if (pos != string::npos) {
        pinAssignments = pinAssignments.substr(pos + 1, 1);
        if (pinAssignments == "C") {
            return DisplayPortAltModePinAssignment::C;
        } else if (pinAssignments == "D") {
            return DisplayPortAltModePinAssignment::D;
        } else if (pinAssignments == "E") {
            return DisplayPortAltModePinAssignment::E;
        }
    }
    return DisplayPortAltModePinAssignment::NONE;
}

LinkTrainingStatus parseLinkTrainingStatusHelper(string linkTrainingStatus) {
    linkTrainingStatus = Trim(linkTrainingStatus);
    if (linkTrainingStatus == LINK_TRAINING_STATUS_SUCCESS) {
        return LinkTrainingStatus::SUCCESS;
    } else if (linkTrainingStatus == LINK_TRAINING_STATUS_FAILURE ||
               linkTrainingStatus == LINK_TRAINING_STATUS_FAILURE_SINK) {
        return LinkTrainingStatus::FAILURE;
    }
    return LinkTrainingStatus::UNKNOWN;
}

bool isDisplayPortPlug(string vdoString) {
    unsigned long vdo;
    unsigned long receptacleFlag = 1 << DISPLAYPORT_CAPABILITIES_RECEPTACLE_BIT;

    vdoString = Trim(vdoString);
    if (ParseUint(vdoString.c_str(), &vdo)) {
        /* We check to see if receptacleFlag is 0, meaning that the DP interface is presented on a
         * USB-C plug.
         */
        return !(vdo & receptacleFlag);
    } else {
        ALOGE("usbdp: isDisplayPortPlug: errno:%d", errno);
    }

    return false;
}

/**
 * constructAltModeData()
 *
 * Constructs DisplayPortAltModeData for framework layer propagation.
 *
 * Input
 * @hpd: hpd state
 * @pin_assignment: selected pin assignment
 * @link_status: link training status
 * @vdo: port partner displayport alt mode vdo
 *
 * Return:
 * DisplayPortAltModeData structure
 */
AltModeData::DisplayPortAltModeData constructAltModeData(string hpd, string pin_assignment,
                                                         string link_status, string vdo) {
    AltModeData::DisplayPortAltModeData dpData;

    // vdo
    if (isDisplayPortPlug(vdo)) {
        dpData.cableStatus = DisplayPortAltModeStatus::CAPABLE;
    } else {
        dpData.partnerSinkStatus = DisplayPortAltModeStatus::CAPABLE;
    }

    // hpd, status
    if (!strncmp(hpd.c_str(), "1", strlen("1"))) {
        dpData.hpd = true;
    }

    // pin
    dpData.pinAssignment = parsePinAssignmentHelper(pin_assignment);

    // link training
    link_status = Trim(link_status);
    dpData.linkTrainingStatus = parseLinkTrainingStatusHelper(link_status);
    if (dpData.linkTrainingStatus == LinkTrainingStatus::SUCCESS) {
        dpData.partnerSinkStatus = dpData.partnerSinkStatus == DisplayPortAltModeStatus::CAPABLE
                                           ? DisplayPortAltModeStatus::ENABLED
                                           : DisplayPortAltModeStatus::UNKNOWN;
        dpData.cableStatus = dpData.cableStatus == DisplayPortAltModeStatus::CAPABLE
                                     ? DisplayPortAltModeStatus::ENABLED
                                     : DisplayPortAltModeStatus::UNKNOWN;
        if (dpData.partnerSinkStatus == DisplayPortAltModeStatus::ENABLED) {
            dpData.cableStatus = DisplayPortAltModeStatus::ENABLED;
        }
    } else if (dpData.linkTrainingStatus == LinkTrainingStatus::FAILURE &&
               dpData.partnerSinkStatus == DisplayPortAltModeStatus::CAPABLE) {
        // 2.0 cable that fails EDID reports not capable, other link training failures assume
        // 3.0 cable that fails in all other cases.
        dpData.cableStatus = (link_status == LINK_TRAINING_STATUS_FAILURE_SINK)
                                     ? DisplayPortAltModeStatus::NOT_CAPABLE
                                     : DisplayPortAltModeStatus::CAPABLE;
    }

    return dpData;
}

/* Primary Poll Work */
void UsbDp::displayPortPollWorkHelper() {
    /* epoll fields */
    int epoll_fd;
    struct epoll_event ev_hpd, ev_pin, ev_orientation, ev_eventfd, ev_link;
    struct epoll_event ev_debounce, ev_activate;
    int epoll_flags = EPOLLIN | EPOLLET;
    int epoll_nevents = 0;
    /* fd fields */
    int hpd_fd, pin_fd, orientation_fd, link_training_status_fd;
    int fd_flags = O_RDONLY;
    /* DisplayPort Link Setup statuses */
    bool orientationSet = false, pinSet = false;
    int activateRetryCount = 0;
    /* File paths */
    string displayPortUsbPath, irqHpdCountPath, hpdPath, pinAssignmentPath, orientationPath;
    string tcpcBus, linkPath, partnerActivePath, portActivePath;
    /* Other */
    unsigned long res;
    int ret = 0;

    mPollRunning = true;
    mPollStarting = false;

    /*---------- Setup ----------*/

    if (getDisplayPortUsbPathHelper(&displayPortUsbPath) == Status::ERROR) {
        ALOGE("usbdp: worker: could not locate usb displayport directory");
        goto usb_path_error;
    }

    ALOGI("usbdp: worker: displayport usb path located at %s", displayPortUsbPath.c_str());
    hpdPath = displayPortUsbPath + "hpd";
    pinAssignmentPath = displayPortUsbPath + "pin_assignment";
    orientationPath = "/sys/class/typec/port0/orientation";
    linkPath = string(mDrmPath) + "link_status";

    partnerActivePath = displayPortUsbPath + "../mode1/active";
    portActivePath = "/sys/class/typec/port0/port0.0/mode1/active";

    if (mClientPath.empty()) {
        ALOGE("usbdp: worker: mClientPath not defined");
        goto bus_client_error;
    }

    irqHpdCountPath = mClientPath + "irq_hpd_count";
    ALOGI("usbdp: worker: irqHpdCountPath:%s", irqHpdCountPath.c_str());

    epoll_fd = epoll_create(64);
    if (epoll_fd == -1) {
        ALOGE("usbdp: worker: epoll_create failed; errno=%d", errno);
        goto epoll_fd_error;
    }

    if ((hpd_fd = displayPortPollOpenFileHelper(hpdPath.c_str(), fd_flags)) == -1) {
        goto hpd_fd_error;
    }
    if ((pin_fd = displayPortPollOpenFileHelper(pinAssignmentPath.c_str(), fd_flags)) == -1) {
        goto pin_fd_error;
    }
    if ((orientation_fd = displayPortPollOpenFileHelper(orientationPath.c_str(), fd_flags)) == -1) {
        goto orientation_fd_error;
    }
    if ((link_training_status_fd = displayPortPollOpenFileHelper(linkPath.c_str(), fd_flags)) ==
        -1) {
        goto link_training_status_fd_error;
    }

    // Set epoll_event events and flags
    epoll_flags = EPOLLIN | EPOLLET;
    ev_hpd.events = epoll_flags;
    ev_pin.events = epoll_flags;
    ev_orientation.events = epoll_flags;
    ev_eventfd.events = epoll_flags;
    ev_link.events = epoll_flags;
    ev_debounce.events = epoll_flags;
    ev_activate.events = epoll_flags;

    ev_hpd.data.fd = hpd_fd;
    ev_pin.data.fd = pin_fd;
    ev_orientation.data.fd = orientation_fd;
    ev_eventfd.data.fd = mDisplayPortEventPipe;
    ev_link.data.fd = link_training_status_fd;
    ev_debounce.data.fd = mDisplayPortDebounceTimer;
    ev_activate.data.fd = mActivateTimer;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, hpd_fd, &ev_hpd) == -1) {
        ALOGE("usbdp: worker: epoll_ctl failed to add hpd; errno=%d", errno);
        goto error;
    }
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, pin_fd, &ev_pin) == -1) {
        ALOGE("usbdp: worker: epoll_ctl failed to add pin; errno=%d", errno);
        goto error;
    }
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, orientation_fd, &ev_orientation) == -1) {
        ALOGE("usbdp: worker: epoll_ctl failed to add orientation; errno=%d", errno);
        goto error;
    }
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, link_training_status_fd, &ev_link) == -1) {
        ALOGE("usbdp: worker: epoll_ctl failed to add link status; errno=%d", errno);
        goto error;
    }
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, mDisplayPortDebounceTimer, &ev_debounce) == -1) {
        ALOGE("usbdp: worker: epoll_ctl failed to add framework update debounce; errno=%d", errno);
        goto error;
    }
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, mActivateTimer, &ev_activate) == -1) {
        ALOGE("usbdp: worker: epoll_ctl failed to add activate debounce; errno=%d", errno);
        goto error;
    }
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, mDisplayPortEventPipe, &ev_eventfd) == -1) {
        ALOGE("usbdp: worker: epoll_ctl failed to add event pipe; errno=%d", errno);
        goto error;
    }

    /* Arm timer to see if DisplayPort Alt Mode Activates */
    armTimerFdHelper(mActivateTimer, DISPLAYPORT_ACTIVATE_DEBOUNCE_MS);

    while (!destroyDisplayPortThread) {
        struct epoll_event events[64];

        epoll_nevents = epoll_wait(epoll_fd, events, 64, -1);
        if (epoll_nevents == -1) {
            if (errno == EINTR)
                continue;
            ALOGE("usbdp: worker: epoll_wait failed; errno=%d", errno);
            break;
        }

        for (int n = 0; n < epoll_nevents; n++) {
            if (events[n].data.fd == hpd_fd) {
                if (!pinSet || !orientationSet) {
                    ALOGW("usbdp: worker: HPD may be set before pin_assignment and orientation");
                    if (!pinSet && writeDisplayPortAttribute("pin_assignment", pinAssignmentPath) ==
                                           Status::SUCCESS) {
                        pinSet = true;
                    }
                    if (!orientationSet &&
                        writeDisplayPortAttribute("orientation", orientationPath) ==
                                Status::SUCCESS) {
                        orientationSet = true;
                    }
                }
                writeDisplayPortAttribute("hpd", hpdPath);
                armTimerFdHelper(mDisplayPortDebounceTimer, DISPLAYPORT_STATUS_DEBOUNCE_MS);
            } else if (events[n].data.fd == pin_fd) {
                if (writeDisplayPortAttribute("pin_assignment", pinAssignmentPath) ==
                    Status::SUCCESS) {
                    pinSet = true;
                    armTimerFdHelper(mDisplayPortDebounceTimer, DISPLAYPORT_STATUS_DEBOUNCE_MS);
                }
            } else if (events[n].data.fd == orientation_fd) {
                if (writeDisplayPortAttribute("orientation", orientationPath) == Status::SUCCESS) {
                    orientationSet = true;
                    armTimerFdHelper(mDisplayPortDebounceTimer, DISPLAYPORT_STATUS_DEBOUNCE_MS);
                }
            } else if (events[n].data.fd == link_training_status_fd) {
                armTimerFdHelper(mDisplayPortDebounceTimer, DISPLAYPORT_STATUS_DEBOUNCE_MS);
            } else if (events[n].data.fd == mDisplayPortDebounceTimer) {
                ret = read(mDisplayPortDebounceTimer, &res, sizeof(res));
                ALOGI("usbdp: dp debounce triggered, val:%lu ret:%d", res, ret);
                if (ret < 0) {
                    ALOGW("usbdp: debounce read errno:%d", errno);
                    continue;
                }
                if (mCallback) {
                    mCallback(mPayload);
                }
            } else if (events[n].data.fd == mActivateTimer) {
                string activePartner, activePort;

                if (ReadFileToString(partnerActivePath.c_str(), &activePartner) &&
                    ReadFileToString(portActivePath.c_str(), &activePort)) {
                    // Retry activate signal when DisplayPort Alt Mode is active on port but not
                    // partner.
                    if (!strncmp(activePartner.c_str(), "no", strlen("no")) &&
                        !strncmp(activePort.c_str(), "yes", strlen("yes")) &&
                        activateRetryCount < DISPLAYPORT_ACTIVATE_MAX_RETRIES) {
                        if (!WriteStringToFile("1", partnerActivePath)) {
                            ALOGE("usbdp: Failed to activate port partner Alt Mode");
                        } else {
                            ALOGI("usbdp: Attempting to activate port partner Alt Mode");
                        }
                        activateRetryCount++;
                        armTimerFdHelper(mActivateTimer, DISPLAYPORT_ACTIVATE_DEBOUNCE_MS);
                    } else {
                        ALOGI("usbdp: DisplayPort Alt Mode is active, or disabled on port");
                    }
                } else {
                    activateRetryCount++;
                    armTimerFdHelper(mActivateTimer, DISPLAYPORT_ACTIVATE_DEBOUNCE_MS);
                    ALOGE("usbdp: Failed to read active state from port or partner");
                }
            } else if (events[n].data.fd == mDisplayPortEventPipe) {
                uint64_t flag = 0;
                if (!read(mDisplayPortEventPipe, &flag, sizeof(flag))) {
                    if (errno == EAGAIN)
                        continue;
                    ALOGI("usbdp: worker: Shutdown eventfd read error");
                    goto error;
                }
                if (flag == DISPLAYPORT_SHUTDOWN_SET) {
                    ALOGI("usbdp: worker: Shutdown eventfd triggered");
                    destroyDisplayPortThread = true;
                    break;
                } else if (flag == DISPLAYPORT_IRQ_HPD_COUNT_CHECK) {
                    ALOGI("usbdp: worker: IRQ_HPD event through DISPLAYPORT_IRQ_HPD_COUNT_CHECK");
                    writeDisplayPortAttribute("irq_hpd_count", irqHpdCountPath);
                }
            }
        }
    }

error:
    /* Need to disarm so new threads don't get old event */
    armTimerFdHelper(mDisplayPortDebounceTimer, 0);
    armTimerFdHelper(mActivateTimer, 0);
    close(link_training_status_fd);
link_training_status_fd_error:
    close(orientation_fd);
orientation_fd_error:
    close(pin_fd);
pin_fd_error:
    close(hpd_fd);
hpd_fd_error:
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, mDisplayPortDebounceTimer, &ev_debounce);
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, mActivateTimer, &ev_activate);
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, mDisplayPortEventPipe, &ev_eventfd);
    close(epoll_fd);
epoll_fd_error:
bus_client_error:
usb_path_error:
    mPollRunning = false;
    ALOGI("usbdp: worker: exiting worker thread");
}

}  // namespace usb
}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
