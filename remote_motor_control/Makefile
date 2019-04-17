LOCAL_BUILD=0
ifeq "$(LOCAL_BUILD)" "0"
	CC=${EC535}/arm-linux/bin/arm-linux-gcc
	override CPPFLAGS += -I${EC535}/arm-linux/usr/include
	override LDFLAGS += -L${EC535}/arm-linux/usr/lib
else
	override CPPFLAGS += `pkg-config --cflags dbus-1`
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
override CPPFLAGS += -Wall -Werror -Ithirdparty/include -Isrc

$(binary_name): src/main.o src/bluetooth.o
	$(CC) $(CFLAGS) $(LDFLAGS) $+ -ldbus-1 -lbluetooth -o $@

src/bluetooth.o: src/bluetooth.c src/bluetooth.h
src/main.o: src/main.c src/bluetooth.h

.PHONY: clean
clean:
	rm -f remote_motor_control src/bluetooth.o src/main.o
