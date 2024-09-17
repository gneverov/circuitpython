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

MEMORY
{
    FLASH(rx) : ORIGIN = 0x10000000, LENGTH = 16M
    PSRAM(rwx) : ORIGIN = 0x11000000, LENGTH = 16M
    RAM(rwx) : ORIGIN =  0x20000000, LENGTH = 512k
    SCRATCH_X(rwx) : ORIGIN = 0x20040000, LENGTH = 4k
    SCRATCH_Y(rwx) : ORIGIN = 0x20041000, LENGTH = 4k
}

ENTRY(_entry_point)

SECTIONS
{
    /* The bootrom will enter the image at the point indicated in your
       IMAGE_DEF, which is usually the reset handler of your vector table.

       The debugger will use the ELF entry point, which is the _entry_point
       symbol, and in our case is *different from the bootrom's entry point.*
       This is used to go back through the bootrom on debugger launches only,
       to perform the same initial flash setup that would be performed on a
       cold boot.
    */

    .init : {
        HIDDEN(__flash_binary_start = .);
        HIDDEN(__logical_binary_start = .);
        KEEP (*(.vectors))
        KEEP (*(.binary_info_header))
        HIDDEN(__binary_info_header_end = .);
        KEEP (*(.embedded_block))
        HIDDEN(__embedded_block_end = .);
        KEEP (*(.reset))
    } > FLASH

    .flash_env 0x10001000 (NOLOAD) : { 
        *(.flash_env*)
    } > FLASH =0xff

    .text : ALIGN(4k) {
        KEEP (*(.init .init.*))
        *(.text .text.*)
        KEEP (*(.fini .fini.*))

        /* preinit data */
        . = ALIGN(4);        
        PROVIDE_HIDDEN (__preinit_array_start = .);
		KEEP (*(SORT_BY_NAME(.preinit_array.*)))
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

    /* Note the boot2 section is optional, and should be discarded if there is
       no reference to it *inside* the binary, as it is not called by the
       bootrom. (The bootrom performs a simple best-effort XIP setup and
       leaves it to the binary to do anything more sophisticated.) However
       there is still a size limit of 256 bytes, to ensure the boot2 can be
       stored in boot RAM.

       Really this is a "XIP setup function" -- the name boot2 is historic and
       refers to its dual-purpose on RP2040, where it also handled vectoring
       from the bootrom into the user image.
    */

    .boot2 : {
        __boot2_start__ = .;
        *(.boot2)
        __boot2_end__ = .;
    } > FLASH

    ASSERT(__boot2_end__ - __boot2_start__ <= 256,
        "ERROR: Pico second stage bootloader must be no more than 256 bytes in size")
        
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

    /* Start and end symbols must be word-aligned */
    .scratch_x : {
        __scratch_x_start__ = .;
        *(.scratch_x.*)
        . = ALIGN(4);
        __scratch_x_end__ = .;
    } > SCRATCH_X AT > FLASH
    __scratch_x_source__ = LOADADDR(.scratch_x);

    .scratch_y : {
        __scratch_y_start__ = .;
        *(.scratch_y.*)
        . = ALIGN(4);
        __scratch_y_end__ = .;
    } > SCRATCH_Y AT > FLASH
    __scratch_y_source__ = LOADADDR(.scratch_y);

    /* .stack*_dummy section doesn't contains any symbols. It is only
     * used for linker to calculate size of stack sections, and assign
     * values to stack symbols later
     *
     * stack1 section may be empty/missing if platform_launch_core1 is not used */

    /* by default we put core 0 stack at the end of scratch Y, so that if core 1
     * stack is not used then all of SCRATCH_X is free.
     */
    .stack1_dummy (NOLOAD):
    {
        *(.stack1*)
    } > SCRATCH_X
    .stack_dummy (NOLOAD):
    {
        KEEP(*(.stack*))
    } > SCRATCH_Y

    .flash_end : {
        KEEP(*(.embedded_end_block*))
        PROVIDE_HIDDEN(__flash_binary_end = .);
    } > FLASH

    /* stack limit is poorly named, but historically is maximum heap ptr */
    HIDDEN(__StackLimit = ORIGIN(RAM) + LENGTH(RAM));
    HIDDEN(__StackOneTop = ORIGIN(SCRATCH_X) + LENGTH(SCRATCH_X));
    HIDDEN(__StackTop = ORIGIN(SCRATCH_Y) + LENGTH(SCRATCH_Y));
    HIDDEN(__StackOneBottom = __StackOneTop - SIZEOF(.stack1_dummy));
    HIDDEN(__StackBottom = __StackTop - SIZEOF(.stack_dummy));
    PROVIDE_HIDDEN(__stack = __StackTop);

    HIDDEN(__HeapLimit = __StackLimit);    
    PROVIDE_HIDDEN(__heap_start = __end__);
    PROVIDE_HIDDEN(__heap_end = __HeapLimit);

    .flash_heap (READONLY) : ALIGN(4k) {
        HIDDEN(__flash_heap_start = .);
        *(.flash_heap*)
    } > FLASH
    
    /* Check if data + heap + stack exceeds RAM limit */
    ASSERT(__StackLimit >= __HeapLimit, "region RAM overflowed")

    ASSERT( __binary_info_header_end - __logical_binary_start <= 1024, "Binary info must be in first 1024 bytes of the binary")
    ASSERT( __embedded_block_end - __logical_binary_start <= 4096, "Embedded block must be in first 4096 bytes of the binary")

    /* todo assert on extra code */
}
