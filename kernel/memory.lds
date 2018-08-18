/*
 * This essentially says the executable must be loaded in the 4MB-8MB range.
 * This prevents the linker from placing stuff at 0x08048000, since GRUB
 * loads the filesystem module after the kernel, and we need the filesystem
 * to also lie within the kernel page.
 */
MEMORY
{
    kernel(rwxa) : ORIGIN = 0x400000, LENGTH = 0x400000
}
