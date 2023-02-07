# The zkEVM LLVM Framework

This directory and its sub-directories contain the source code for the zkEVM LLVM framework,
a toolkit for the construction of highly optimized compilers, optimizers, and run-time environments
used by the Solidity and Vyper compilers developed by Matter Labs.

## Overview

Welcome to the zkEVM LLVM project!

The project has multiple components. The core of the project is
itself called "LLVM". This contains all of the tools, libraries, and header
files needed to process intermediate representations and convert them into
object files. Tools include an assembler, disassembler, bitcode analyzer, and
bitcode optimizer. It also contains basic regression tests.

The zkEVM back-end is called `SyncVM`, and the architecture is called `syncvm`.

## Building

The zkEVM LLVM framework must be built with our tool called `zkevm-llvm`:

1. Install some tools system-wide:
   1.a. `apt install cmake ninja-build clang-13 lld-13 parallel` on a Debian-based Linux, with optional `musl-tools` if you need a `musl` build
   1.b. `pacman -S cmake ninja clang lld parallel` on an Arch-based Linux
   1.c. On MacOS, install the [HomeBrew](https://brew.sh) package manager (being careful to install it as the appropriate user), then `brew install cmake ninja coreutils parallel`
   1.d. Their equivalents with other package managers

2. [Install Rust](https://www.rust-lang.org/tools/install)

   Currently we are not pinned to any specific version of Rust, so just install the latest stable build for your platform.
   Also install the `musl` target if you are compiling on Linux in order to distribute the binaries:
   `rustup target add x86_64-unknown-linux-musl`

3. Install the zkEVM LLVM framework builder:
   3.a. `cargo install compiler-llvm-builder` on MacOS, or Linux for personal use
   3.b. `cargo install compiler-llvm-builder --target x86_64-unknown-linux-musl` on Linux for distribution

   The builder is not the zkEVM LLVM framework itself, but a tool that clones its repository and runs the sequence of build commands.
   By default it is installed in `~/.cargo/bin/`, which is recommended to be added to your `$PATH`.

4. Create the `LLVM.lock` file with the URL and branch or tag you want to build. For example:

  ```
  url = "https://github.com/matter-labs-forks/compiler-llvm"
  branch = "v1.3.0"
  ```

5. Run the builder to clone and build the zkevm LLVM framework:
   5.1. `zkevm-llvm clone`
   5.2. `zkevm-llvm build`

   The build artifacts will end up in the `./target-llvm/target-final/` directory.
   You may point your `LLVM_SYS_150_PREFIX` to that directory to use this build as a compiler dependency.

## Troubleshooting

- If you get a “failed to authenticate when downloading repository… if the git CLI succeeds then net.git-fetch-with-cli may help here” error,
then prepending the `cargo` command with `CARGO_NET_GIT_FETCH_WITH_CLI=true` may help.

## License

The zkEVM fork of the LLVM framework is distributed under the terms of
Apache License, Version 2.0 with LLVM Exceptions, ([LICENSE](LICENSE) or <https://llvm.org/LICENSE.txt>)

## Resources

[Official LLVM documentation](https://llvm.org/docs/GettingStarted.html)
