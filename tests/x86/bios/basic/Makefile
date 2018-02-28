ASM       = gcc
CC        = gcc
LD        = ld
OBJDUMP   = objdump
OBJCOPY   = objcopy

CFLAGS    = -m32
LDFLAGS   = -nostartfile

OBJECTS   = bios.o
TARGET    = bios
MEMLAYOUT = xMemLayout.map

.PHONY: all clean

all: $(TARGET).rom

clean:
	@-rm -f -v *.o $(TARGET).out $(MEMLAYOUT) $(TARGET).rom

%.o: %.c Makefile
	@echo "[CC]  $@"
	@$(CC) -c -o $*.o $(CFLAGS) $<

%.o: %.S Makefile
	@echo "[AS]  $<"
	@$(ASM) -c -o $*.o $(CFLAGS) $<

# Produce a disassembly dump of the main section, for verification purposes
dis: $(TARGET).out
	@echo "[DIS] $<"
	@$(OBJCOPY) -O binary -j .main --set-section-flags .main=alloc,load,readonly,code $< main.bin
	@$(OBJDUMP) -D -bbinary -mi8086 -Mintel main.bin | less
	@-rm -f main.bin

$(TARGET).out: $(OBJECTS) $(TARGET).ld
	@echo "[LD]  $@"
	@$(LD) $(LDFLAGS) -T$(TARGET).ld -o $@ $(OBJECTS) -Map $(MEMLAYOUT)

$(TARGET).rom: $(TARGET).out
	@echo "[ROM] $@"
	@# Note: -j only works for sections that have the 'ALLOC' flag set
	@$(OBJCOPY) -O binary -j .begin -j .main -j .reset --gap-fill=0x0ff $< $@
