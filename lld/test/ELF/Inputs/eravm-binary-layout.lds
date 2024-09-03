ENTRY(0)
SECTIONS {
  .code : SUBALIGN(8) {
    *(.text)

    ASSERT((32 - (31 & .)) % 8 == 0, "padding isn't multiple of 8");

    /* Align the .rodata to 32 bytes. */
    . = ALIGN(32);
    *(.rodata)

    ASSERT(. % 32 == 0, "size isn't multiple of 32");

    /* Add padding such that the size be odd number of words */
    . = ((. >> 5) | 1) << 5;

    *(.eravm-metadata)

    ASSERT(. % 64 == 32, "size isn't odd number of words");
  } = 0

  /* .data section itself that contains initializers of global variables,
     is not needed. */
  /DISCARD/ : {
    *(.data)
  }
}
