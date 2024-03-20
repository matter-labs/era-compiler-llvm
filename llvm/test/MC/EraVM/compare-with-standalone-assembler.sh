#!/bin/bash

script_dir="$(dirname "$0")"
llvm_dir="$(realpath "$script_dir/../../../..")"

# Try to guess
assembler_dir="$(realpath "$llvm_dir/../era-zkEVM-assembly")"

llvm_mc="$llvm_dir/build/bin/llvm-mc"
reader="$assembler_dir/target/debug/reader"

echo "Paths to be used:"
echo "llvm-mc: $llvm_mc"
echo "reader:  $reader"

if ! test -x "$llvm_mc"; then
  echo "'llvm-mc' is not executable, exiting!"
  exit 1
fi
if ! test -x "$reader"; then
  echo "'reader' is not executable, exiting!"
  exit 1
fi

# Remove instructions that are *not* expected to be handled
# by the standalone assembler
filter_common() {
  sed -E -e '/code\[/ d'
}

# Obtain hex representation of machine code produced by the LLVM backend:
# aa bb cc dd ee ff 00 11
# 22 33 44 55 66 77 88 99
# ...
#
# First, only select lines containing "...; encoding: [...]" and drop everything
# except the "encoding" payload. Then, drop commas and "0x" prefixes.
mc_bytes() {
  "$llvm_mc" -arch=eravm --show-encoding | \
      grep -F '; encoding: [' | \
      sed -E 's/^.*; encoding: \[(.*)\].*$/\1/' |
      sed -E -e 's/0x//g' -e 's/,/ /g'
}

# Assemble stdin with the standalone assembler and format its output in lines
# of eight hex-encoded bytes.
reader_bytes() {
  "$reader" /dev/stdin | \
      fold -b -w2 | tr '\n' ' ' | \
      fold -b -w24
}

num_lines() {
  wc -l
}

process_one_source() {
  local file_name="$1"

  echo "Test source: $file_name"
  local filtered_src="$(filter_common < $file_name)"
  echo "Filtered lines: $(num_lines <<< "$filtered_src") of $(num_lines < $file_name)"

  local llvm_bytes="$(mc_bytes <<< "$filtered_src")"
  local llvm_num_lines=$(num_lines <<< "$llvm_bytes")
  echo "LLVM MC produced $llvm_num_lines lines"

  echo

  local reference_bytes="$(reader_bytes <<< "$filtered_src")"
  local reference_num_lines=$(num_lines <<< "$reference_bytes")
  echo "Reference assembler produced $reference_num_lines lines, cropping to $llvm_num_lines lines"
  local reference_bytes_cropped="$(head -n $llvm_num_lines <<< "$reference_bytes")"

  echo

  echo "== DIFF BEGIN =="
  diff \
      --unified \
      --color=auto \
      --ignore-trailing-space \
      <(echo "$llvm_bytes") \
      <(echo "$reference_bytes_cropped")
  echo "== DIFF END =="
  echo
}

for src in "$@"; do
  process_one_source "$src"
done
