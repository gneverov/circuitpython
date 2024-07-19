/* Based on GCC ARM embedded samples.
   Defines the following symbols for use by code:
    __exidx_start
    __exidx_end
    __etext
    __data_start__
    __preinit_array_start
    __preinit_array_end
    __init_array_start
    __init_array_end
    __fini_array_start
    __fini_array_end
    __data_end__
    __bss_start__
    __bss_end__
    __end__
    end
    __HeapLimit
    __StackLimit
    __StackTop
    __stack (== StackTop)
*/

FLASH_SIZE = 2048k;
FLASH_STORAGE_SIZE = 720k;
FLASH_ENV_SIZE = 4k;

MEMORY
{
    FLASH(rx) : ORIGIN = 0x10000000, LENGTH = FLASH_SIZE
    RAM(rwx) : ORIGIN =  0x20000000, LENGTH = 256k
    SCRATCH_X(rwx) : ORIGIN = 0x20040000, LENGTH = 4k
    SCRATCH_Y(rwx) : ORIGIN = 0x20041000, LENGTH = 4k
}

ENTRY(_entry_point)

SECTIONS
{
    /* Second stage bootloader is prepended to the image. It must be 256 bytes big
       and checksummed. It is usually built by the boot_stage2 target
       in the Raspberry Pi Pico SDK
    */

    .flash_begin : {
        HIDDEN(__flash_binary_start = .);
    } > FLASH

    .boot2 : {
        HIDDEN(__boot2_start__ = .);
        KEEP (*(.boot2))
        HIDDEN(__boot2_end__ = .);
    } > FLASH

    ASSERT(__boot2_end__ - __boot2_start__ == 256,
        "ERROR: Pico second stage bootloader must be 256 bytes in size")

    /* The second stage will always enter the image at the start of .text.
       The debugger will use the ELF entry point, which is the _entry_point
       symbol if present, otherwise defaults to start of .text.
       This can be used to transfer control back to the bootrom on debugger
       launches only, to perform proper flash setup.
    */

    .text : {
        HIDDEN(__logical_binary_start = .);
        KEEP (*(.vectors))
        KEEP (*(.binary_info_header))
        HIDDEN(__binary_info_header_end = .);
        KEEP (*(.reset))
        /* TODO revisit this now memset/memcpy/float in ROM */
        /* bit of a hack right now to exclude all floating point and time critical (e.g. memset, memcpy) code from
         * FLASH ... we will include any thing excluded here in .data below by default */
        *(.init)
        /* Change for MicroPython... exclude gc.c, parse.c, vm.c from flash */
        *(EXCLUDE_FILE(*libgcc.a: *libc.a: *lib_a-mem*.o *libm.a: *gc.c.obj *vm.c.obj *parse.c.obj) .text*)
        *(.fini)
        /* Pull all c'tors into .text */
        *crtbegin.o(.ctors)
        *crtbegin?.o(.ctors)
        *(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors)
        *(SORT(.ctors.*))
        *(.ctors)
        /* Followed by destructors */
        *crtbegin.o(.dtors)
        *crtbegin?.o(.dtors)
        *(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors)
        *(SORT(.dtors.*))
        *(.dtors)

        *(.eh_frame*)
        . = ALIGN(4);
    } > FLASH

    .rodata : {
        *(EXCLUDE_FILE(*libgcc.a: *libc.a:*lib_a-mem*.o *libm.a:) .rodata*)
        . = ALIGN(4);
        *(SORT_BY_ALIGNMENT(SORT_BY_NAME(.flashdata*)))
        . = ALIGN(4);
    } > FLASH

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    HIDDEN(__exidx_start = .);
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > FLASH
    __exidx_end = .;

    /* Machine inspectable binary information */
    . = ALIGN(4);
    HIDDEN(__binary_info_start = .);
    .binary_info :
    {
        KEEP(*(.binary_info.keep.*))
        *(.binary_info.*)
    } > FLASH
    HIDDEN(__binary_info_end = .);
    . = ALIGN(4);

    /* End of .text-like segments */
    HIDDEN(__etext = .);

   .ram_vector_table (COPY): {
        *(.ram_vector_table)
    } > RAM

    .data : {
        HIDDEN(__data_start__ = .);
        *(vtable)

        *(.time_critical*)

        /* remaining .text and .rodata; i.e. stuff we exclude above because we want it in RAM */
        *(.text*)
        . = ALIGN(4);
        *(.rodata*)
        . = ALIGN(4);

        *(.data*)

        . = ALIGN(4);
        *(.after_data.*)
        . = ALIGN(4);
        /* preinit data */
        PROVIDE_HIDDEN (__mutex_array_start = .);
        KEEP(*(SORT(.mutex_array.*)))
        KEEP(*(.mutex_array))
        PROVIDE_HIDDEN (__mutex_array_end = .);

        . = ALIGN(4);
        /* preinit data */
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP(*(SORT(.preinit_array.*)))
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN (__preinit_array_end = .);

        . = ALIGN(4);
        /* init data */
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN (__init_array_end = .);

        . = ALIGN(4);
        /* finit data */
        PROVIDE_HIDDEN (__fini_array_start = .);
        *(SORT(.fini_array.*))
        *(.fini_array)
        PROVIDE_HIDDEN (__fini_array_end = .);
    } > RAM AT> FLASH

	.tdata : {
        HIDDEN(__tdata_source = LOADADDR(.tdata));
        HIDDEN(__tls_base = ADDR(.tdata));
        HIDDEN(__tdata_start = .);
		*(.tdata .tdata.* .gnu.linkonce.td.*)

        . = ALIGN(4);
        /* All data end */
        HIDDEN(__data_end__ = .);        
		HIDDEN(__tdata_end = .);
	} > RAM AT> FLASH

    .flash_end : {
        HIDDEN(__flash_binary_end = .);
    } > FLASH

	.tbss (NOLOAD) : {
        HIDDEN(__bss_start__ = .);
        HIDDEN(__tbss_start = .);
		*(.tbss .tbss.* .gnu.linkonce.tb.*)
		*(.tcommon)
		PROVIDE( __tls_end = . );
		PROVIDE( __tbss_end = . );
	} > RAM

	PROVIDE_HIDDEN( __tbss_offset = ADDR(.tbss) - ADDR(.tdata) );
	PROVIDE_HIDDEN( __tls_size = __tls_end - __tls_base );
	PROVIDE_HIDDEN( __tls_align = MAX(ALIGNOF(.tdata), ALIGNOF(.tbss)) );
	PROVIDE_HIDDEN( __tls_size_align = (__tls_size + __tls_align - 1) & ~(__tls_align - 1));
	PROVIDE_HIDDEN( __arm32_tls_tcb_offset = MAX(8, __tls_align) );
	PROVIDE_HIDDEN( __arm64_tls_tcb_offset = MAX(16, __tls_align) );

    .uninitialized_data (COPY): {
        . = ALIGN(4);
        *(.uninitialized_data*)
    } > RAM

    /* bss without zero init on startup */
    .uninitialized_bss (NOLOAD): {
        . = ALIGN(4);
        *(.uninitialized_bss*)
    } > RAM

    .bss (NOLOAD) : {
        . = ALIGN(4);
        
        *(SORT_BY_ALIGNMENT(SORT_BY_NAME(.bss*)))
        *(COMMON)
        . = ALIGN(4);
        HIDDEN(__bss_end__ = .);

        HIDDEN(__end__ = .);
        HIDDEN(end = __end__);
    } > RAM

    /* Scratch regions are used as MSP stack and are thus not initialized.
     * These symbols are needed by pico sdk's crt0, but result in a no-op.
     */
    HIDDEN(__scratch_x_start__ = ORIGIN(SCRATCH_X));
    HIDDEN(__scratch_x_end__ = ORIGIN(SCRATCH_X));
    HIDDEN(__scratch_x_source__ = ORIGIN(SCRATCH_X));
    HIDDEN(__scratch_y_start__ = ORIGIN(SCRATCH_Y));
    HIDDEN(__scratch_y_end__ = ORIGIN(SCRATCH_Y));
    HIDDEN(__scratch_y_source__ = ORIGIN(SCRATCH_Y));

    /* These symbols define the MSP stacks (one per core). The MSP stack is 
     * only used for "main" before the FreeRTOS scheduler starts, and for 
     * interrupts.
     */
    HIDDEN(__StackLimit = ORIGIN(RAM) + LENGTH(RAM));
    HIDDEN(__HeapLimit = __StackLimit);    
    HIDDEN(__StackOneTop = ORIGIN(SCRATCH_X) + LENGTH(SCRATCH_X));
    HIDDEN(__StackTop = ORIGIN(SCRATCH_Y) + LENGTH(SCRATCH_Y));
    HIDDEN(__StackOneBottom = ORIGIN(SCRATCH_X));
    HIDDEN(__StackBottom = ORIGIN(SCRATCH_Y));
    PROVIDE_HIDDEN(__stack = __StackTop);

    .flash_heap (READONLY) : ALIGN(4k) {
        HIDDEN(__flash_heap_start = .);
        *(.flash_heap*)
        HIDDEN(__flash_heap_end = ORIGIN(FLASH) + LENGTH(FLASH) - FLASH_STORAGE_SIZE - FLASH_ENV_SIZE);
    } > FLASH
    
    HIDDEN(__flash_storage_end = ORIGIN(FLASH) + LENGTH(FLASH) - FLASH_ENV_SIZE);
    HIDDEN(__flash_storage_start = __flash_storage_end - FLASH_STORAGE_SIZE);
    ASSERT(__flash_storage_start >= ., "flash region overflowed")

    .flash_env __flash_storage_end (COPY) : ALIGN(4k) {
        *(.flash_env*)
    } > FLASH

    ASSERT( __binary_info_header_end - __logical_binary_start <= 256, "Binary info must be in first 256 bytes of the binary")
    /* todo assert on extra code */
}
