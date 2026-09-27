#ifndef CHARDEV_H
#define CHARDEV_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define CPCIDEV_MAGIC 'c'
#define DEVICE_FILE_NAME "cpcidev_pci"

#define IOCTL_SET_OP1_MATRIX _IOW(CPCIDEV_MAGIC, 1, uint32_t[4][4])
#define IOCTL_SET_OP2_MATRIX _IOW(CPCIDEV_MAGIC, 2, uint32_t[4][4])
#define IOCTL_GET_RESULT     _IOR(CPCIDEV_MAGIC, 3, uint32_t[4][4])
#define IOCTL_SET_OPCODE     _IOW(CPCIDEV_MAGIC, 4, uint32_t)

#endif