TARGET = nvparm
TARGET_LIB = nvparm.a

GCC = $(CROSS_COMPILE)gcc
STRIP =$(CROSS_COMPILE)strip

SRC += $(wildcard *.c littlefs/*.c)
OBJ := $(SRC:.c=.o)
DEP := $(SRC:.c=.d)

ifdef DEBUG
override CFLAGS += -O0 -g3
else
override CFLAGS += -Os
endif

override CFLAGS += -I. -Ilittlefs
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

all: $(TARGET) $(TARGET_LIB)

-include $(DEP)

$(TARGET): $(OBJ)
	$(GCC) $(CFLAGS) $^ $(LFLAGS) -o $@
ifndef DEBUG
ifndef UNSTRIPPED
	@echo "  STRIP   $@"
	$(STRIP) $@
endif
endif

%.o: %.c
	$(GCC) -c -MMD $(CFLAGS) $< -o $@

$(TARGET_LIB): $(OBJ)
	$(AR) rcs $@ $^

clean:
	rm -f $(TARGET)
	rm -f $(OBJ)
	rm -f $(DEP)
