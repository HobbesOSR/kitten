/* 
 * 2021, Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef __HAFNIUM_H__
#define __HAFNIUM_H__

#include <stdint.h>

#define HAFNIUM_IOCTL_HYP_INIT      100
#define HAFNIUM_IOCTL_LAUNCH_VM     101
#define HAFNIUM_IOCTL_STOP_VM       102

#define HAFNIUM_CMD_PATH "/dev/hafnium"

struct hafnium_cmd {
    uint64_t cmd;
    uint32_t data_len;
    uint8_t  data[0];
} __attribute__((packed));


#endif