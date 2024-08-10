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
        KEEP (*(.embedded_block))
        HIDDEN(__embedded_block_end = .);
        KEEP (*(.reset))

        KEEP (*(.init .init.*))
        *(.text .text.*)
        KEEP (*(.fini .fini.*))

        /* preinit data */
        . = ALIGN(4);        
        PROVIDE_HIDDEN (__preinit_array_start = .);
		KEEP (*(SORT_BY_INIT_PRIORITY(.preinit_array.*)))
		KEEP (*(.preinit_array))
        PROVIDE_HIDDEN (__preinit_array_end = .);

        /* init data */
        . = ALIGN(4);        
        PROVIDE_HIDDEN (__init_array_start = .);
		KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
		KEEP (*(.init_array .ctors))
        PROVIDE_HIDDEN (__init_array_end = .);

        /* fini data */
        . = ALIGN(4);        
        PROVIDE_HIDDEN (__fini_array_start = .);
		KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
		KEEP (*(.fini_array .dtors))
        PROVIDE_HIDDEN (__fini_array_end = .);

        *(.eh_frame*);
    } > FLASH

    .rodata : {
        *(.rodata .rodata.*)
        *(SORT_BY_ALIGNMENT(SORT_BY_NAME(.flashdata*)))
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

    /* End of .text-like segments */
    . = ALIGN(4);    
    HIDDEN(__etext = .);

   .ram_vector_table (COPY): {
        *(.ram_vector_table)
    } > RAM

    .uninitialized_data (NOLOAD) : {
        *(.uninitialized_data*)
    } > RAM

    .data : ALIGN_WITH_INPUT {
        HIDDEN(__data_start__ = .);
        *(.time_critical*)
        *(.data .data.*)
        *(.after_data.*)

        /* mutex data */
        . = ALIGN(4);
        PROVIDE_HIDDEN (__mutex_array_start = .);
        KEEP(*(SORT(.mutex_array.*)))
        KEEP(*(.mutex_array))
        PROVIDE_HIDDEN (__mutex_array_end = .);
    } > RAM AT> FLASH

	.tdata : ALIGN_WITH_INPUT {
        HIDDEN(__tdata_source = LOADADDR(.tdata));
        HIDDEN(__tls_base = ADDR(.tdata));
        HIDDEN(__tdata_start = .);
		*(.tdata .tdata.*)

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
		*(.tbss .tbss.*)
		HIDDEN( __tls_end = . );
		HIDDEN( __tbss_end = . );
	} > RAM

	PROVIDE_HIDDEN( __tbss_offset = ADDR(.tbss) - ADDR(.tdata) );
	PROVIDE_HIDDEN( __tls_size = __tls_end - __tls_base );
	PROVIDE_HIDDEN( __tls_align = MAX(ALIGNOF(.tdata), ALIGNOF(.tbss)) );
	PROVIDE_HIDDEN( __tls_size_align = (__tls_size + __tls_align - 1) & ~(__tls_align - 1));
	PROVIDE_HIDDEN( __arm32_tls_tcb_offset = MAX(8, __tls_align) );
	PROVIDE_HIDDEN( __arm64_tls_tcb_offset = MAX(16, __tls_align) );

	/*
	 * Unlike ld.lld, ld.bfd does not advance the location counter for
	 * .tbss, but we actually need memory allocated for .tbss as we use
	 * it for the initial TLS storage.
	 * Create special section here just to make room.
	 */
    .tbss_space (NOLOAD) : {
		. = ADDR(.tbss);
		. = . + SIZEOF(.tbss);
	} > RAM

    .bss (NOLOAD) : {
        *(.bss .bss.*)
        /* bss_end needs alignment because of Pico SDK's crt0 implementation */
        .  = ALIGN(4);
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
    PROVIDE_HIDDEN(__heap_start = __end__);
    PROVIDE_HIDDEN(__heap_end = __HeapLimit);

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
