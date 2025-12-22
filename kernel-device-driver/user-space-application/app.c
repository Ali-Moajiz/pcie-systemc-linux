/*
 * Minimal userspace application for CPCIDEV
 * Demonstrates 4x4 matrix multiplication
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>

// #define IOCTL_SET_OP1_MATRIX 0x01
// #define IOCTL_SET_OP2_MATRIX 0x02
// #define IOCTL_GET_RESULT 0x03
// #define IOCTL_SET_OPCODE 0x04

#define CPCIDEV_MAGIC 'c'

#define IOCTL_SET_OP1_MATRIX _IOW(CPCIDEV_MAGIC, 1, uint32_t[4][4])
#define IOCTL_SET_OP2_MATRIX _IOW(CPCIDEV_MAGIC, 2, uint32_t[4][4])
#define IOCTL_GET_RESULT _IOR(CPCIDEV_MAGIC, 3, uint32_t[4][4])
#define IOCTL_SET_OPCODE _IOW(CPCIDEV_MAGIC, 4, uint32_t)

int main(void)
{
    int fd;
    int ret;
    printf("[APP]: IOCTL_SET_OP1_MATRIX = 0x%x\n", IOCTL_SET_OP1_MATRIX);
    printf("[APP]: IOCTL_SET_OP2_MATRIX = 0x%x\n", IOCTL_SET_OP2_MATRIX);
    printf("[APP]: IOCTL_GET_RESULT     = 0x%x\n", IOCTL_GET_RESULT);
    printf("[APP]: IOCTL_SET_OPCODE     = 0x%x\n", IOCTL_SET_OPCODE);

    uint32_t A[4][4] = {
        {122, 2, 3, 4},
        {57, 6, 7, 82},
        {9, 171, 252, 37},
        {4, 52, 6, 7}};

    uint32_t B[4][4] = {
        {1, 100, 0, 0},
        {100, 1, 0, 0},
        {0, 20, 1, 0},
        {0, 0, 0, 301}};

    /*
    RESULT MUST BE...
        uint32_t C[4][4] = {
    {  322, 12262,   3,  1204},
    {  657,  5846,   7, 24682},
    {17109,  6111, 252, 11137},
    { 5204,   572,   6,  2107}
};


    */

    uint32_t C[4][4] = {0};
    uint32_t opcode = 1; // matrix multiplication

    fd = open("/dev/cpcidev_pci", O_RDWR);
    if (fd < 0)
    {
        perror("open");
        return 1;
    }

    printf("[APP]: Device opened\n");

    /* Write OP1 matrix */
    ret = ioctl(fd, IOCTL_SET_OP1_MATRIX, A);
    if (ret < 0)
    {
        perror("[APP]: IOCTL_SET_OP1_MATRIX failed");
        close(fd);
        return 1;
    }
    printf("[APP]: OP1 matrix written\n");

    /* Write OP2 matrix */
    printf("[APP]: About to call IOCTL_SET_OP2_MATRIX with cmd=0x%x\n", IOCTL_SET_OP2_MATRIX);
    ret = ioctl(fd, IOCTL_SET_OP2_MATRIX, B);
    printf("[APP]: ioctl returned: %d\n", ret);
    if (ret < 0)
    {
        perror("[APP]: IOCTL_SET_OP2_MATRIX failed");
        close(fd);
        return 1;
    }
    printf("[APP]: OP2 matrix written\n");

    /* Set opcode */
    ret = ioctl(fd, IOCTL_SET_OPCODE, &opcode);
    if (ret < 0)
    {
        perror("[APP]: IOCTL_SET_OPCODE failed");
        close(fd);
        return 1;
    }
    printf("[APP]: Opcode set to %u\n", opcode);

    /* Read result */
    ret = ioctl(fd, IOCTL_GET_RESULT, C);
    if (ret < 0)
    {
        perror("[APP]: IOCTL_GET_RESULT failed");
        close(fd);
        return 1;
    }
    printf("[APP]: Result matrix retrieved\n");

    /* Print result */
    printf("[APP]: Result matrix:\n");
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            printf("%6u ", C[i][j]);
        }
        printf("\n");
    }

    close(fd);
    return 0;
}