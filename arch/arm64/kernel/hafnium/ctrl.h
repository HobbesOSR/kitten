/* 
 * 2021, Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef __CTRL_H__
#define __CTRL_H__

#define HAFNIUM_IOCTL_HYP_INIT      100
#define HAFNIUM_IOCTL_LAUNCH_VM     101
#define HAFNIUM_IOCTL_STOP_VM       102


struct hafnium_cmd {
    uint64_t cmd;
    uint32_t data_len;
    uint8_t  data[0];
} __attribute__((packed));

#endif