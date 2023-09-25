# zkSync Era: The zkEVM LLVM Framework

[![Logo](eraLogo.svg)](https://zksync.io/)

zkSync Era is a layer 2 rollup that uses zero-knowledge proofs to scale Ethereum without compromising on security
or decentralization. As it's EVM-compatible (with Solidity/Vyper), 99% of Ethereum projects can redeploy without
needing to refactor or re-audit any code. zkSync Era also uses an LLVM-based compiler that will eventually enable
developers to write smart contracts in popular languages such as C++ and Rust.

This directory and its sub-directories contain the source code for the zkEVM fork of the [LLVM](https://llvm.org) framework,
a toolkit for the construction of highly optimized compilers, optimizers, and run-time environments
used by the Solidity and Vyper compilers developed by Matter Labs.

## Overview

Welcome to the zkEVM LLVM project!

The project has multiple components. The core of the project is
the `llvm` directory. This contains all of the tools, libraries, and header
files needed to process intermediate representations and convert them into
object files. Tools include an assembler, disassembler, bitcode analyzer, and
bitcode optimizer. These tools are not yet officially supported for third-party front-ends.
It also contains zkEVM modifications of the standard [LLVM regression tests](https://llvm.org/docs/TestingGuide.html#regression-tests).

The zkEVM back-end is called `EraVM`, and the architecture is called `eravm`.

## Building

The zkEVM LLVM framework must be built with our tool called `zkevm-llvm`:

1. Install some tools system-wide:  
   1.a. `apt install cmake ninja-build clang-13 lld-13` on a Debian-based Linux, with optional `musl-tools` if you need a `musl` build  
   1.b. `pacman -S cmake ninja clang lld` on an Arch-based Linux  
   1.c. On MacOS, install the [HomeBrew](https://brew.sh) package manager (being careful to install it as the appropriate user), then `brew install cmake ninja coreutils`. Install your choice of a recent LLVM/[Clang](https://clang.llvm.org) compiler, e.g. via [Xcode](https://developer.apple.com/xcode/), [Apple’s Command Line Tools](https://developer.apple.com/library/archive/technotes/tn2339/_index.html), or your preferred package manager.  
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

4. In a directory in which you want the `llvm` directory, create an `LLVM.lock` file with the URL and branch or tag you want to build. For example:

  ```
  url = "<THIS REPO URL>"
  branch = "<THIS REPO BRANCH>"
  ```

5. Run the builder to clone and build the zkevm LLVM framework:  
   5.1. `zkevm-llvm clone`  
   5.2. `zkevm-llvm build`  

   The build artifacts will end up in the `./target-llvm/target-final/` directory.
   You may point your `LLVM_SYS_150_PREFIX` to that directory to use this build as a compiler dependency.
   If built with the `--enable-tests` option, test tools will be in the `./target-llvm/build-final/` directory, along with copies of the build artifacts.

## Troubleshooting

- If you get a “failed to authenticate when downloading repository… if the git CLI succeeds then net.git-fetch-with-cli may help here” error,
then prepending the `cargo` command with `CARGO_NET_GIT_FETCH_WITH_CLI=true` may help.
- Unset any LLVM-related environment variables you may have set.

## License

The zkEVM fork of the LLVM framework is distributed under the terms of
Apache License, Version 2.0 with LLVM Exceptions, ([LICENSE](LICENSE) or <https://llvm.org/LICENSE.txt>)

## Resources

[Official LLVM documentation](https://llvm.org/docs/GettingStarted.html)

## Official Links

- [Website](https://zksync.io/)
- [GitHub](https://github.com/matter-labs)
- [Twitter](https://twitter.com/zksync)
- [Twitter for Devs](https://twitter.com/zkSyncDevs)
- [Discord](https://join.zksync.dev/)

## Disclaimer

zkSync Era has been through extensive testing and audits, and although it is live, it is still in alpha state and
will undergo further audits and bug bounty programs. We would love to hear our community's thoughts and suggestions
about it!
It's important to note that forking it now could potentially lead to missing important
security updates, critical features, and performance improvements.
