GROUP(libc.a libg.a libnosys.a)
INCLUDE app_gnu.ld
/* Specify the memory areas */
MEMORY
{
  TCM (xw)      : ORIGIN = 0x00000000, LENGTH = 480K
  MEMTOP(w)      : ORIGIN = 0x00180000, LENGTH = 40K
}

IPC_BUFFER_POOL_SZ = 1k;
boot_stack_size = DEFINED(boot_stack_size) ? boot_stack_size : 0x800;
exception_stack_size = DEFINED(exception_stack_size) ? exception_stack_size : 0x200;
fiq_stack_size = DEFINED(fiq_stack_size) ? fiq_stack_size : 0x200 ;

/* Define output sections */
SECTIONS
{
	.text :
	{
		__text_start__ = .;
		KEEP(*(.isr_vector))
		*(.text*)
		*(.rodata*)
		__text_end__ = .;
	} > TCM

	.data :
	{
		__data_start__ = .;
		*(vtable)
		*(.data*)
		. = ALIGN(4);
		__data_end__ = .;
	} > TCM

	/* Variable to determine the size of Erika binary */
	erika_size = .;

	.bss :
	{
		. = ALIGN(4);
		__bss_start__ = .;
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);
		__bss_end__ = .;
	} > TCM

	.boot_stack :
	{
		. = ALIGN(8);
		. = . + boot_stack_size;
		__boot_stack__ = .;
	} > TCM

	.excetption_stack :
	{
		. = ALIGN(8);
		. = . + exception_stack_size;
		__exception_stack__ = .;
	} > TCM

	.fiq_stack :
	{
		. = ALIGN(8);
		. = . + fiq_stack_size;
		__fiq_stack__ = .;
	} > TCM

    /* The rest of the DTCM is used for malloc */
    system_malloc_baseaddr = .;
    system_malloc_endaddr = ORIGIN(TCM) + LENGTH(TCM) - IPC_BUFFER_POOL_SZ;

	IPC_BUFFER_POOL_START = system_malloc_endaddr;

    /* Ethernet memory */
    eth_malloc_baseaddr = ORIGIN(MEMTOP);
    eth_malloc_endaddr = ORIGIN(MEMTOP) + LENGTH(MEMTOP);
}
