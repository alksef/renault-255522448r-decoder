/* libopencm3 supplies vector table, startup, data/bss initialisation. */
MEMORY
{
    rom (rx) : ORIGIN = 0x08000000, LENGTH = 32K
    ram (rwx) : ORIGIN = 0x20000000, LENGTH = 6K
}
INCLUDE cortex-m-generic.ld
ASSERT((_stack - _ebss) >= 2048, "Less than 2 KiB reserved for stack")
