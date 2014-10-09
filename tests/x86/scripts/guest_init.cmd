##########################################
# Sample Batch script to create a guest. #
##########################################

# Create the guest's device tree node
devtree node copy /guests guest0 /templates/x86_64_generic

# Create the guest itself
guest create guest0

# Load the guest bios
vfs guest_load guest0 0x80000 /coreboot.rom

# Load guest bios in higher alias
vfs guest_load guest0 0xfff80000 /coreboot.rom
