SECTIONS {
  .code : SUBALIGN(8) {
    *(.init.data)
    *(.text)
    *(.landingpads)

    /* Align the .rodata to 32 bytes. */
    ASSERT((32 - (31 & .)) % 8 == 0, "aligment should be a multiple of 8");
    . = (. + 31) & ~31;
    *(.rodata)

    /* Make the section size the even number of words (32 bytes each). */
    ASSERT((64 - (63 & .)) % 32 == 0, "alignment should be a multiple of 32");
    . = (. + 63) & ~63;
  } = 0

  /* .data section itself, that contains initializers of global variables,
     is not needed. */
  /DISCARD/ : {
    *(.data)
  }
}
