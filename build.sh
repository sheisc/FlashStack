#!/bin/bash

CUR_LLVM_DIR=llvm-7.0.0


if [ ! -d $CUR_LLVM_DIR ]; then
    wget http://releases.llvm.org/7.0.0/clang+llvm-7.0.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz
    tar -xvf clang+llvm-7.0.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz
    mv clang+llvm-7.0.0-x86_64-linux-gnu-ubuntu-16.04 llvm-7.0.0
    rm -f clang+llvm-7.0.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz	
fi





make -C FlashStack

# curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
# rustup self uninstall

# rustup toolchain install 1.43
# rustup default 1.43