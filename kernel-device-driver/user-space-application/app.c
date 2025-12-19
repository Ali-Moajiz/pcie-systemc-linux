/*
 * Minimal userspace application for CPCIDEV
 * Demonstrates 4x4 matrix multiplication
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>

#include "../chardev.h"
#define IOCTL_SET_OP1_MATRIX   0x01
#define IOCTL_SET_OP2_MATRIX   0x02
#define IOCTL_GET_RESULT      0x03
#define IOCTL_SET_OPCODE      0x04

int main(void)
{
    int fd;
    uint32_t A[4][4] = {
        {1, 2, 3, 4},
        {5, 6, 7, 8},
        {9, 1, 2, 3},
        {4, 5, 6, 7}
    };

    uint32_t B[4][4] = {
        {1, 0, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 1}
    };

    uint32_t C[4][4] = {0};
    uint32_t opcode = 1; // matrix multiplication

    fd = open("/dev/cpcidev_pci", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    printf("Device opened\n");

    /* Write matrices */
    ioctl(fd, IOCTL_SET_OP1_MATRIX, A);
    ioctl(fd, IOCTL_SET_OP2_MATRIX, B);

    /* Set opcode */
    ioctl(fd, IOCTL_SET_OPCODE, &opcode);

    /* Read result */
    ioctl(fd, IOCTL_GET_RESULT, C);

    /* Print result */
    printf("Result matrix:\n");
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            printf("%6u ", C[i][j]);
        }
        printf("\n");
    }

    close(fd);
    return 0;
}
