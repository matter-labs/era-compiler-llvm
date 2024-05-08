# zkSync Era: The ZKsync LLVM Framework

[![Logo](eraLogo.svg)](https://zksync.io/)

zkSync Era is a layer 2 rollup that uses zero-knowledge proofs to scale Ethereum without compromising on security
or decentralization. As it's EVM-compatible (with Solidity/Vyper), 99% of Ethereum projects can redeploy without
needing to refactor or re-audit any code. zkSync Era also uses an LLVM-based compiler that will eventually enable
developers to write smart contracts in popular languages such as C++ and Rust.

This directory and its sub-directories contain the source code for the ZKsync fork of the [LLVM](https://llvm.org) framework,
a toolkit for the construction of highly optimized compilers, optimizers, and run-time environments
used by the Solidity and Vyper compilers developed by Matter Labs.

## Overview

Welcome to the ZKsync LLVM project!

The project has multiple components. The core of the project is
the `llvm` directory. This contains all of the tools, libraries, and header
files needed to process intermediate representations and convert them into
object files. Tools include an assembler, disassembler, bitcode analyzer, and
bitcode optimizer. These tools are not yet officially supported for third-party front-ends.
It also contains ZKsync modifications of the standard [LLVM regression tests](https://llvm.org/docs/TestingGuide.html#regression-tests).

## Building

The ZKsync LLVM framework must be built with our tool called `zkevm-llvm`:

<details>
<summary>1. Install the system prerequisites.</summary>

   * Linux (Debian):

      Install the following packages:
      ```shell
      apt install cmake ninja-build curl git libssl-dev pkg-config clang lld
      ```
   * Linux (Arch):

      Install the following packages:
      ```shell
      pacman -Syu which cmake ninja curl git pkg-config clang lld
      ```

   * MacOS:

      * Install the [HomeBrew](https://brew.sh) package manager.
      * Install the following packages:

         ```shell
         brew install cmake ninja coreutils
         ```

      * Install your choice of a recent LLVM/[Clang](https://clang.llvm.org) compiler, e.g. via [Xcode](https://developer.apple.com/xcode/), [Apple’s Command Line Tools](https://developer.apple.com/library/archive/technotes/tn2339/_index.html), or your preferred package manager.
</details>

<details>
<summary>2. Install Rust.</summary>

   * Follow the latest [official instructions]((https://www.rust-lang.org/tools/install)):
      ```shell
      curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
      . ${HOME}/.cargo/env
      ```

      > Currently we are not pinned to any specific version of Rust, so just install the latest stable build for your   platform.
</details>

<details>
<summary>3. Install the ZKsync LLVM framework builder.</summary>

   * Install the builder using `cargo`:
      ```shell
      cargo install compiler-llvm-builder
      ```

      > The builder is not the ZKsync LLVM framework itself, but a tool that clones its repository and runs a sequence of build commands. By default it is installed in `~/.cargo/bin/`, which is recommended to be added to your `$PATH`.

</details>

<details>
<summary>4. Create the `LLVM.lock` file.</summary>

   * In a directory in which you want the `llvm` directory, create an `LLVM.lock` file with the URL and branch or tag you want to build, for example:

      ```properties
      url = "https://github.com/matter-labs/era-compiler-llvm"
      branch = "main"
      ```

</details>

<details>
<summary>5. Build LLVM.</summary>

   * Clone and build the ZKsync LLVM framework using the `zkevm-llvm` tool:
      ```shell
      zkevm-llvm clone
      zkevm-llvm build
      ```

      The build artifacts will end up in the `./target-llvm/target-final/` directory.
      You may point your `LLVM_SYS_170_PREFIX` to that directory to use this build as a compiler dependency.
      If built with the `--enable-tests` option, test tools will be in the `./target-llvm/build-final/` directory, along   with copies of the build artifacts. For all supported build options, run `zkevm-llvm build --help`.

</details>

## Troubleshooting

- Make sure your system is up-to-date.
- Make sure you have the required system prerequisites installed.
- Unset any LLVM-related environment variables you may have set.
- If you encounter any problems with the building process, please open an issue in the [era-compiler-llvm](https://github.com/matter-labs/era-compiler-llvm/issues) repository.

## License

The ZKsync fork of the LLVM framework is distributed under the terms of
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
