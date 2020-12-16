FLAGS = -static -fno-builtin -nostdlib -nostdinc -r -mkernel -mlong-branch -Wl,-kext
DEFINES = -DKERNEL
INCLUDES = -I /System/Library/Frameworks/Kernel.framework/Headers
WARNINGS = -Wall

PREFIX=/System/Library/Extensions
BUNDLE=$(PREFIX)/yes.kext

all: yes

install: yes Info.plist
	mkdir -p $(BUNDLE)/Contents/MacOS/
	install yes $(BUNDLE)/Contents/MacOS/
	install Info.plist $(BUNDLE)/Contents/

uninstall:
	rm -r $(BUNDLE)

yes: yes.c
	cc $^ -o $@ $(FLAGS) $(INCLUDES) $(WARNINGS) $(DEFINES)

clean:
	rm -rf yes yes.kext

.PHONY: clean
