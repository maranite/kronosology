CC      ?= gcc
CFLAGS  := -std=gnu99 -m32 -fno-pic -fno-PIE -fno-PIC -nostdlib -fno-builtin \
           -Os -fomit-frame-pointer -ffreestanding
LDFLAGS := -m32 -nostdlib -Wl,-e,_start -Wl,-no-pie -Wl,-z,norelro
TARGET  := InstallEXs

all: $(TARGET)

$(TARGET): installexs.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
	strip $@

clean:
	rm -f $(TARGET)

# Copy built binary into the auto-auth root (where install.sh picks it up)
install: $(TARGET)
	cp $(TARGET) ../$(TARGET)_bin
