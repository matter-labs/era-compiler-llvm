ENTRY(0)
SECTIONS {
  .code : SUBALIGN(8) {
    *(.text)

    ASSERT((32 - (31 & .)) % 8 == 0, "padding isn't multiple of 8");
    /* Check the code size is no more than 2^16 instructions. */
    ASSERT(. <= (1 << 16) * 8, "number of instructions > 2^16");
    /* Align the .rodata to 32 bytes. */
    . = (. + 31) & ~31;
    *(.rodata)

    ASSERT(. % 32 == 0, "size isn't multiple of 32");

    /* Add padding with metadata */
    . = ((((. + 32) >> 5) | 1 ) << 5) - 13;

    BYTE(0x3A);
    BYTE(0xC2);
    BYTE(0x25);
    BYTE(0x16);
    BYTE(0x8D);
    BYTE(0xF5);
    BYTE(0x42);
    BYTE(0x12);
    BYTE(0xA2);
    BYTE(0x5C);
    BYTE(0x1C);
    BYTE(0x01);
    BYTE(0xFD);

    ASSERT(. % 64 == 32, "size isn't odd number of words");

    /* Check the total binary size is no more than (2^16 - 2) words. */
    ASSERT(. <= ((1 << 16) - 1) * 32, "Binary size > (2^16 - 2) words")
  } = 0

  /* .data section itself that contains initializers of global variables,
   is not needed. */
  /DISCARD/ : {
    *(.data)
  }
}
