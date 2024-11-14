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

__linker_symbol__$9d134d75c24f6705416dcd739f310469be$__0 = 0x0a0a0a0a;
__linker_symbol__$9d134d75c24f6705416dcd739f310469be$__1 = 0x0a0a0a0a;
__linker_symbol__$9d134d75c24f6705416dcd739f310469be$__2 = 0x0a0a0a0a;
__linker_symbol__$9d134d75c24f6705416dcd739f310469be$__3 = 0x0a0a0a0a;
__linker_symbol__$9d134d75c24f6705416dcd739f310469be$__4 = 0x0a0a0a0a;

__factory_dependency__$9d134d75c24f6705416dcd739f310469be$__0 = 0x01010101;
__factory_dependency__$9d134d75c24f6705416dcd739f310469be$__1 = 0x02020202;
__factory_dependency__$9d134d75c24f6705416dcd739f310469be$__2 = 0x03030303;
__factory_dependency__$9d134d75c24f6705416dcd739f310469be$__3 = 0x04040404;
__factory_dependency__$9d134d75c24f6705416dcd739f310469be$__4 = 0x05050505;
__factory_dependency__$9d134d75c24f6705416dcd739f310469be$__5 = 0x06060606;
__factory_dependency__$9d134d75c24f6705416dcd739f310469be$__6 = 0x07070707;
__factory_dependency__$9d134d75c24f6705416dcd739f310469be$__7 = 0x08080808;
