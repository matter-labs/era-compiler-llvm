FROM debian:buster-slim

WORKDIR /usr/src/zksync

# Install required dependencies
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        make bash git openssl curl sudo wget cmake \
        libssl-dev gcc g++ libpq-dev pkg-config \
        software-properties-common jq

RUN wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add - && \
    apt-add-repository "deb http://apt.llvm.org/buster/ llvm-toolchain-buster-11 main" && \
    apt-get install -y clang-11 lld-11

# Install Rust and required cargo packages
ENV RUSTUP_HOME=/usr/local/rustup \
    CARGO_HOME=/usr/local/cargo \
    PATH=/usr/local/cargo/bin:$PATH
RUN curl https://sh.rustup.rs -sSf | bash -s -- -y
RUN cargo install diesel_cli --no-default-features --features postgres
RUN cargo install --version=0.5.6 sqlx-cli

# Install `solc`
RUN curl -LO https://github.com/ethereum/solidity/releases/download/v0.5.16/solc-static-linux
RUN chmod +x solc-static-linux
RUN mv solc-static-linux /usr/local/bin/solc

# WORKDIR ${HOME}/opt/solc/
# RUN mkdir -pv ${HOME}/opt/solc/ \
#     && wget 'https://github.com/ethereum/solc-bin/raw/gh-pages/linux-amd64/solc-linux-amd64-v0.8.0%2Bcommit.c7dfd78e' --output-document "solc-0.8.0" \
#     && wget 'https://github.com/ethereum/solc-bin/raw/gh-pages/linux-amd64/solc-linux-amd64-v0.8.1%2Bcommit.df193b15' --output-document "solc-0.8.1" \
#     && wget 'https://github.com/ethereum/solc-bin/raw/gh-pages/linux-amd64/solc-linux-amd64-v0.8.2%2Bcommit.661d1103' --output-document "solc-0.8.2" \
#     && wget 'https://github.com/ethereum/solc-bin/raw/gh-pages/linux-amd64/solc-linux-amd64-v0.8.3%2Bcommit.8d00100c' --output-document "solc-0.8.3" \
#     && wget 'https://github.com/ethereum/solc-bin/raw/gh-pages/linux-amd64/solc-linux-amd64-v0.8.4%2Bcommit.c7e474f2' --output-document "solc-0.8.4" \
#     && wget 'https://github.com/ethereum/solc-bin/raw/gh-pages/linux-amd64/solc-linux-amd64-v0.8.5%2Bcommit.a4f2e591' --output-document "solc-0.8.5" \
#     && wget 'https://github.com/ethereum/solc-bin/raw/gh-pages/linux-amd64/solc-linux-amd64-v0.8.6%2Bcommit.11564f7e' --output-document "solc-0.8.6" \
#     && wget 'https://github.com/ethereum/solc-bin/raw/gh-pages/linux-amd64/solc-linux-amd64-v0.8.7%2Bcommit.e28d00a7' --output-document "solc-0.8.7" \
#     && PATH="${HOME}/opt/solc/:${PATH}"

# can be needed in some cases
# - name: Symlink libclang.so (Linux)
#   run: sudo ln -s libclang-11.so.1 /lib/x86_64-linux-gnu/libclang.so
#   working-directory: ${{ env.LLVM_PATH }}/lib

# Setup the environment
ENV ZKSYNC_HOME=/usr/src/zksync
ENV PATH="${ZKSYNC_HOME}/bin:${PATH}"

RUN cargo install sccache

ENV RUSTC_WRAPPER=/usr/local/cargo/bin/sccache
ENV SCCACHE_DIR=/usr/src/cache/sccache
WORKDIR /usr/src/zksync