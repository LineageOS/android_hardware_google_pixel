/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __BROWNOUT_STATS_H
#define __BROWNOUT_STATS_H

#define METER_CHANNEL_MAX     12
#define DATA_LOGGING_LEN      20
#define TRIGGERED_SOURCE_MAX  17

struct odpm_instant_data {
    struct timespec time;
    unsigned int value[METER_CHANNEL_MAX];
};

/* Notice: sysfs only allocates a buffer of PAGE_SIZE
 * so the sizeof brownout_stats should be smaller than that
 */
struct brownout_stats {
    struct timespec triggered_time;
    unsigned int triggered_idx;

    struct odpm_instant_data main_odpm_instant_data[DATA_LOGGING_LEN];
    struct odpm_instant_data sub_odpm_instant_data[DATA_LOGGING_LEN];
    unsigned int triggered_state[DATA_LOGGING_LEN];
};

#endif /* __BROWNOUT_STATS_H */
