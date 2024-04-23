/* Not a real EraVM memory layout. */
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
