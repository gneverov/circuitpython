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
    FLASH(rx) : ORIGIN = 0x10000000, LENGTH = 2048k
    RAM(rwx) : ORIGIN =  0x20000000, LENGTH = 256k
}

SECTIONS
{
    .text : {
        *(.text .text.*)

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

        /* object data */
        . = ALIGN(4);
        HIDDEN(__init_object_start = .);
        KEEP(*(.init_object))
        HIDDEN(__init_object_end = .);

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

    __exidx_start = .;
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > FLASH
    __exidx_end = .;

    /* End of .text-like segments */
    HIDDEN(__etext = .);

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

        /* All data end */
        HIDDEN(__data_end__ = .);
    } > RAM AT> FLASH
    
    .uninitialized_data (NOLOAD) : {
        *(.uninitialized_data*)
    } > RAM

    /* bss without zero init on startup */
    .uninitialized_bss (NOLOAD) : {
        *(.uninitialized_bss*)
    } > RAM

    .bss (NOLOAD) : {
        HIDDEN(__bss_start__ = .);
        *(.bss .bss.*)
        /* bss_end needs alignment because of Pico SDK's crt0 implementation */
        .  = ALIGN(4);        
        HIDDEN(__bss_end__ = .);
    } > RAM
}
