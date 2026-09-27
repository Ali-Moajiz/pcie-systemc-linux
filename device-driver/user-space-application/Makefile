CC ?= gcc

CFLAGS ?= -Wall -O2
TARGET := custom_device_app

SRCS := app.c
OBJS := $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c chardev.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean