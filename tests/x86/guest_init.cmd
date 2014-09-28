# Create the guest device tree node by copying from host.
devtree node copy /guests guest0 /templates/x86_64_generic

#Create the guest.
guest create guest0

#Copy guest BIOS
vfs guest_load guest0 0x80000 /coreboot.rom
vfs guest_load guest0 0xfff80000 /coreboot.rom
