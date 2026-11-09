NDK ?= /home/unknown/Android/Sdk/ndk/28.2.13676358
CC := $(NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android34-clang
STRIP := $(NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip

CFLAGS := -O3 -Wall -Wextra -static -flto -D_GNU_SOURCE
LDFLAGS := -static -flto

SRCS := main.c config.c actions.c device.c http_server.c
OBJS := $(SRCS:.c=.o)
TARGET := keyactiond

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
	$(STRIP) --strip-all $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
