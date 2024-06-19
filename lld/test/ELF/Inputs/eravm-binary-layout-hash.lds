SECTIONS {
  .code : SUBALIGN(8) {
    *(.text)

    ASSERT((32 - (31 & .)) % 8 == 0, "padding isn't multiple of 8");

    /* Align the .rodata to 32 bytes. */
    . = (. + 31) & ~31;
    *(.rodata)

    ASSERT(. % 32 == 0, "size isn't multiple of 32");

    /* Add padding and hash, such that the size be odd number of words */
    . = (. + 63) & ~63;
    LONG(0x1625c23a);
    LONG(0x1242f58d);
    LONG(0x011c5ca2);
    LONG(0xbfbe35fd);
    LONG(0xda8f40ea);
    LONG(0xdd1de3c2);
    LONG(0xbba4806f);
    LONG(0xcbf1a5f9);

    ASSERT(. % 64 == 32, "size isn't odd number of words");
  } = 0

  /* .data section itself that contains initializers of global variables,
     is not needed. */
  /DISCARD/ : {
    *(.data)
  }
}
