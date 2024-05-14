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

#ifndef HARDWARE_GOOGLE_PIXEL_USB_USBDPUTILS_H_
#define HARDWARE_GOOGLE_PIXEL_USB_USBDPUTILS_H_

#include <aidl/android/hardware/usb/AltModeData.h>
#include <aidl/android/hardware/usb/DisplayPortAltModePinAssignment.h>
#include <aidl/android/hardware/usb/DisplayPortAltModeStatus.h>
#include <aidl/android/hardware/usb/LinkTrainingStatus.h>
#include <aidl/android/hardware/usb/Status.h>

#include <string>

using aidl::android::hardware::usb::AltModeData;
using aidl::android::hardware::usb::DisplayPortAltModePinAssignment;
using aidl::android::hardware::usb::LinkTrainingStatus;
using aidl::android::hardware::usb::Status;

using std::string;

#define DISPLAYPORT_SHUTDOWN_CLEAR 0
#define DISPLAYPORT_SHUTDOWN_SET 1
#define DISPLAYPORT_IRQ_HPD_COUNT_CHECK 3

#define DISPLAYPORT_POLL_WAIT_MS 100

#define SVID_DISPLAYPORT "ff01"
#define SVID_THUNDERBOLT "8087"

namespace android {
namespace hardware {
namespace google {
namespace pixel {
namespace usb {

class UsbDp {
  private:
    string mDrmPath;
    string mClientPath;

    // True when mPoll thread is running
    volatile bool mPollRunning;
    volatile bool mPollStarting;

    pthread_cond_t mCV;
    pthread_mutex_t mCVLock;

    volatile bool mFirstSetupDone;

    // Used to cache the values read from tcpci's irq_hpd_count.
    // Update drm driver when cached value is not the same as the read value.
    uint32_t mIrqCountCache;

    pthread_t mPoll;
    pthread_t mDisplayPortShutdownHelper;

    // Callback called when mDisplayPortDebounceTimer is triggered
    void (*mCallback)(void *payload);
    void *mPayload;

    // eventfd to signal DisplayPort thread from typec kernel driver
    int mDisplayPortEventPipe;

    /*
     * eventfd to set DisplayPort framework update debounce timer. Debounce
     * timer is necessary for
     *     1) allowing enough time for each sysfs node needed to set HPD high
     *        in the drm to populate
     *     2) preventing multiple IRQs that trigger link training failures
     *        from continuously sending notifications to the frameworks layer.
     */
    int mDisplayPortDebounceTimer;

    /*
     * eventfd to monitor whether a connection results in DisplayPort Alt Mode activating.
     */
    int mActivateTimer;

    /*
     * Indicates whether or not port partner supports DisplayPort, and is used to
     * communicate to the drm when the port partner physically disconnects.
     */
    bool mPartnerSupportsDisplayPort;

    Status writeDisplayPortAttribute(string attribute, string usb_path);

  public:
    UsbDp(const char *const drmPath);

    /* Internal to Library */
    // For thread setup
    void displayPortPollWorkHelper();
    void shutdownDisplayPortPollHelper();

    /* For HAL Use */
    // Protects writeDisplayPortAttribute(), setupDisplayPortPoll(),
    // and shutdownDisplayPortPoll()
    pthread_mutex_t mLock;

    // Setup and Shutdown Thread
    void setupDisplayPortPoll();
    void shutdownDisplayPortPoll(bool force);

    bool isFirstSetupDone();
    // i2cClientPath
    void setClientPath(string path);
    // mPollRunning
    bool getPollRunning();
    // mPartnerSupportsDisplayPort
    void setPartnerSupportsDisplayPort(bool supportsDp);
    bool getPartnerSupportsDisplayPort();

    void updateDisplayPortEventPipe(uint64_t flag);

    Status readDisplayPortAttribute(string attribute, string usb_path, string *value);
    Status writeHpdOverride(string attribute, string value);

    void registerCallback(void (*callback)(void *(payload)), void *payload);
};

/* Sysfs Helper Functions */
Status getDisplayPortUsbPathHelper(string *path);
Status queryPartnerSvids(std::vector<string> *svids);

/* AIDL Helper Functions */
AltModeData::DisplayPortAltModeData constructAltModeData(string hpd, string pin_assignment,
                                                         string link_status, string vdo);

}  // namespace usb
}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_PIXEL_USB_USBDPUTILS_H_
