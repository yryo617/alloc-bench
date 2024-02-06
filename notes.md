## FreeBSD arm-64 memory mapping

Address space layout.

ARMv8 implements up to a 48 bit virtual address space. The address space is
split into 2 regions at each end of the 64 bit address space, with an
out of range "hole" in the middle.

We use the full 48 bits for each region, however the kernel may only use
a limited range within this space.

Upper region:    0xffff_ffff_ffff_ffff  Top of virtual memory

                 0xffff_feff_ffff_ffff  End of DMAP
                 0xffff_fa00_0000_0000  Start of DMAP

                 0xffff_007f_ffff_ffff  End of KVA
                 0xffff_0000_0000_0000  Kernel base address & start of KVA

Hole:            0xfffe_ffff_ffff_ffff
                 0x0001_0000_0000_0000

Lower region:    0x0000_ffff_ffff_ffff End of user address space
                 0x0000_0000_0000_0000 Start of user address space

We use the upper region for the kernel, and the lower region for userland.

We define some interesting address constants:

VM_MIN_ADDRESS and VM_MAX_ADDRESS define the start and end of the entire
64 bit address space, mostly just for convenience.

VM_MIN_KERNEL_ADDRESS and VM_MAX_KERNEL_ADDRESS define the start and end of
mappable kernel virtual address space.

VM_MIN_USER_ADDRESS and VM_MAX_USER_ADDRESS define the start and end of the
user address space.
 */
