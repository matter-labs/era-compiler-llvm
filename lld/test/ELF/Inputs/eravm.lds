/* Not a real EraVM memory layout. */
ENTRY(0)
MEMORY {
  code (rx)   : ORIGIN = 0,  LENGTH = 4K
  stack (w!x) : ORIGIN = 4K, LENGTH = 4K
}

SECTIONS {
  .code : {
    *(.rodata)
    *(.text)
  } > code
  .stack : {
    *(.data)
  } > stack
}

__$9d134d75c24f6705416dcd739f310469be$__0 = 0x01010101;
__$9d134d75c24f6705416dcd739f310469be$__1 = 0x02020202;
__$9d134d75c24f6705416dcd739f310469be$__2 = 0x03030303;
__$9d134d75c24f6705416dcd739f310469be$__3 = 0x04040404;
__$9d134d75c24f6705416dcd739f310469be$__4 = 0x05050505;
