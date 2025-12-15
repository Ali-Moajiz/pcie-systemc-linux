# Step 1: Set up environment variables
cd ~/pcie-systemc-linux/buildroot

export ARCH=x86_64
export CROSS_COMPILE=$(pwd)/output/host/bin/x86_64-buildroot-linux-gnu-
export KERNEL_SRC=$(pwd)/output/build/linux-6.12.47

# Step 2: Verify the paths
echo "ARCH: $ARCH"
echo "CROSS_COMPILE: $CROSS_COMPILE"
echo "KERNEL_SRC: $KERNEL_SRC"

# Verify compiler exists
${CROSS_COMPILE}gcc --version

# Step 3: Navigate to your module directory and build
cd ~/pcie-systemc-linux/kernel-device-driver

# Clean previous builds
make clean

# Build the kernel module
make -C $KERNEL_SRC M=$(pwd) ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE modules

# Step 4: Verify the build
ls -lh custom_qemu_device_driver.ko
file custom_qemu_device_driver.ko