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

#define LOG_TAG "libpixelusb-usbbushelper"

#include "include/pixelusb/UsbBusHelper.h"

#include <dirent.h>
#include <utils/Log.h>

namespace android {
namespace hardware {
namespace google {
namespace pixel {
namespace usb {

static string getBusNumberString(const string busType, const string busPath) {
    DIR *dp = opendir(busPath.c_str());

    if (dp != NULL) {
        struct dirent *ep;

        while ((ep = readdir(dp))) {
            if (ep->d_type == DT_DIR) {
                // Supposed that there is only one sub dir in the busType pattern
                if (string::npos != string(ep->d_name).find(string(busType + "-"))) {
                    std::strtok(ep->d_name, "-");
                    string busNumber = std::strtok(NULL, "-");
                    closedir(dp);
                    return busNumber;
                }
            }
        }
        closedir(dp);
        ALOGE("Failed to find the %s sub dir under %s", busType.c_str(), busPath.c_str());
        return string("");
    }

    ALOGE("Failed to open %s", busPath.c_str());
    return string("");
}

/*
 * getBusClientPath: Return the full path of the USB Bus client directory
 *
 * The two bus interfaces being used are I2c and SPMI. They can be returned
 * in the following formats:
 *   I2c:
 *     client ID: /sys/devices/platform/10d60000.hsi2c/i2c-7/7-0025/
 *     device name: /sys/devices/platform/10d60000.hsi2c/i2c-7/i2c-max77759tcpc/
 *   SPMI:
 *     client ID: /sys/devices/platform/53f1000.spmi/spmi-0/0-04/
 *
 * For I2c, the bus number and client directory name differs across kernel
 * versions and build targets. Search the bus number first to locate
 * the first level of the sub directory, and then search the I2c device name
 * under it.
 *
 * Append the I2c device name to the full path if found. Otherwise for I2c and
 * SPMI, append: busNumber + "-" + clientId. clientId is a 4-digit number with
 * 0 stuffed in the type of string for I2c, or a 2-digit number for SPMI.
 *
 */
string getBusClientPath(const string busType, const string busPath, const string devName,
                        const string clientId) {
    DIR *dp;
    string strBusNumber, busPathPartial, busClientPath;

    strBusNumber = getBusNumberString(busType, busPath);
    if (strBusNumber.empty()) {
        return string("");
    }

    busPathPartial = busPath + "/" + busType + "-" + strBusNumber;
    dp = opendir(busPathPartial.c_str());
    if (dp != NULL) {
        struct dirent *ep;
        string busClientDevice = strBusNumber + "-" + clientId;

        while ((ep = readdir(dp))) {
            if (ep->d_type == DT_DIR) {
                if (!devName.empty() && string::npos != string(ep->d_name).find(devName)) {
                    closedir(dp);
                    return string(busPathPartial + "/" + devName + "/");
                }
                if (string::npos != string(ep->d_name).find(busClientDevice)) {
                    closedir(dp);
                    return string(busPathPartial + "/" + busClientDevice + "/");
                }
            }
        }
        closedir(dp);
    }

    ALOGE("Failed to open %s", busPathPartial.c_str());
    return string("");
}

}  // namespace usb
}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
