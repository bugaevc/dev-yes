# You have to set SERENITY_ROOT and CXX.
#
# For instance,
# SERENITY_ROOT=/home/me/serenity/
# CXX=/home/me/serenity/Toolchain/Local/i686/bin/i686-pc-serenity-g++

INCLUDES = -nostdinc -nostdinc++ \
	-I "$(SERENITY_ROOT)" \
	-I "$(SERENITY_ROOT)/Userland/Libraries" \
	-I "$(SERENITY_ROOT)/Userland/Libraries/LibC" \
	-I "${SERENITY_ROOT}/Build/i686" \
	-I "${SERENITY_ROOT}/Toolchain/Local/i686/Kernel/i686-pc-serenity/include/c++/11.1.0" \
	-I "${SERENITY_ROOT}/Toolchain/Local/i686/Kernel/i686-pc-serenity/include/c++/11.1.0/i686-pc-serenity"

DEFINES = -D DEBUG -D ENABLE_COMPILETIME_FORMAT_CHECK -D SANITIZE_PTRS \
	-D KERNEL -D __serenity__
WARNINGS = -Wall -Wno-unused-variable -Wno-literal-suffix

FLAGS = -m32 -ffreestanding -pie -fPIE -fbuiltin \
	-mno-80387 -mno-mmx -mno-sse -mno-sse2 \
	-fno-asynchronous-unwind-tables -nostdlib \
	-fno-exceptions -fno-rtti \
	-fstack-protector-strong -fsanitize=undefined \
	-fno-use-cxa-atexit \
	-std=c++2a -fconcepts -Os -g1

YesModule.a: YesDevice.o YesModule.o
	ar rcs $@ $^

YesDevice.o: YesDevice.cpp YesDevice.h
	$(CXX) -x c++ -c -o $@ $(FLAGS) $(INCLUDES) $(DEFINES) $(WARNINGS) YesDevice.cpp

YesModule.o: YesModule.cpp
	$(CXX) -x c++ -c -o $@ $(FLAGS) $(INCLUDES) $(DEFINES) $(WARNINGS) YesModule.cpp

clean:
	rm -f *.o *.a

.PHONY: clean
