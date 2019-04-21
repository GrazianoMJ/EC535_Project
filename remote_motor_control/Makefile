TARGET=gumstix
ifeq "$(TARGET)" "gumstix"
	CC=${EC535}/gumstix/oe/cross/bin/arm-linux-gcc
	override CPPFLAGS += -I${EC535}/gumstix/bluez/bluez-libs-dev/usr/include/
	override LDFLAGS += -L${EC535}/gumstix/bluez/bluez-libs-dev/usr/lib/
else ifeq "$(TARGET)" "emulator"
	CC=${EC535}/arm-linux/bin/arm-linux-gcc
	override CPPFLAGS += -I${EC535}/arm-linux/usr/include
	override LDFLAGS += -L${EC535}/arm-linux/usr/lib
else ifeq "$(TARGET)" "local"
	# Local build
	override CPPFLAGS += `pkg-config --cflags dbus-1`
else
$(error TARGET must be one of gumstix, emulator, or local)
endif

SELF_TEST=0
ifeq "$(SELF_TEST)" "1"
	override CPPFLAGS += -DSELF_TEST
endif

binary_name=remote_motor_control

BUILD_TYPE ?= Debug
ifeq "$(BUILD_TYPE)" "Release"
	override CPPFLAGS += -O3 -DNDEBUG
else ifeq "$(BUILD_TYPE)" "Debug"
	override CPPFLAGS += -g -O0
endif
override CFLAGS += -std=gnu99
override CXXFLAGS += -std=c++11
override CPPFLAGS += -Wall -Werror -Isrc

$(binary_name): src/main.o src/bluetooth.o
	$(CC) $(CFLAGS) $(LDFLAGS) $+ -ldbus-1 -lbluetooth -o $@

src/bluetooth.o: src/bluetooth.c src/bluetooth.h
src/main.o: src/main.c src/bluetooth.h

.PHONY: clean
clean:
	rm -f remote_motor_control src/bluetooth.o src/main.o
