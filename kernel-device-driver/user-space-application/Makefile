# Buildroot toolchain path - adjust this to your actual buildroot output path
BUILDROOT_PATH = $(HOME)/pcie-systemc-linux/buildroot
CROSS_COMPILE = $(BUILDROOT_PATH)/output/host/bin/x86_64-linux-

CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -O2
TARGET = custom_device_app

# Source files
SRCS = app.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c chardev.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean