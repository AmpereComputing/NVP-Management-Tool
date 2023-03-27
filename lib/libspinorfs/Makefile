TARGET_LIB = libspinorfs.a

GCC = $(CROSS_COMPILE)gcc
STRIP =$(CROSS_COMPILE)strip

TOPDIR = ../..
INCDIR = $(TOPDIR)/include

SRC += $(wildcard *.c littlefs/*.c)
OBJ := $(SRC:.c=.o)
DEP := $(SRC:.c=.d)

ifdef DEBUG
override CFLAGS += -O0 -g3
else
override CFLAGS += -Os
endif

override CFLAGS += -I. -Ilittlefs
override CFLAGS += -I$(INCDIR)
override CFLAGS += -std=c99 -Wall -pedantic
override CFLAGS += -Wextra -Wshadow -Wjump-misses-init -Wundef
# Remove missing-field-initializers because of GCC bug
override CFLAGS += -Wno-missing-field-initializers
override CFLAGS += -D_XOPEN_SOURCE=600
override CFLAGS += -D'UN_USED(x)=(void)(x)'
ifdef DEBUG
override CFLAGS += -DDEBUG
endif

# OPENBMC
ifeq ($(CROSS_COMPILE),arm-openbmc-linux-gnueabi-)
override CFLAGS += -ffreestanding -march=armv7-a -mfpu=vfpv4-d16 -mfloat-abi=hard
ifneq ($(SYSROOT),)
override CFLAGS += --sysroot=$(SYSROOT)
endif
endif

all: $(TARGET_LIB)

-include $(DEP)

%.o: %.c
	$(GCC) -c -MMD $(CFLAGS) $< -o $@

$(TARGET_LIB): $(OBJ)
	$(AR) rcs $@ $^

clean:
	rm -f $(TARGET_LIB)
	rm -f $(OBJ)
	rm -f $(DEP)
