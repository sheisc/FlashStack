export AFL_PATH=/home/iron/github/ParallelShadowStacks/FlashStack
export AFL_RUSTC=/home/iron/.rustup/toolchains/1.43-x86_64-unknown-linux-gnu/bin/rustc

export LC_ALL=C
#export __SPA_PROTECTED_FUNCS_PATH=/home/iron/github/ParallelShadowStacks/FlashStack/protected_funcs.txt

#export AFL_KEEP_ASSEMBLY=1
#export CC=$AFL_PATH/afl-clang
#export CXX=$AFL_PATH/afl-clang++
export LLVM_INSTALL_PATH=/home/iron/src/SP2021/llvm-7.0.0
export PATH=$LLVM_INSTALL_PATH/bin:$PATH
export PATH=$AFL_PATH:$PATH
export LD_PRELOAD=/home/iron/github/ParallelShadowStacks/FlashStack/libfsgs.so


# export AFL_CXX=$LLVM_INSTALL_PATH/bin/clang++
# export AFL_CC=$LLVM_INSTALL_PATH/bin/clang
#export AFL_CC=/usr/bin/gcc
#export AFL_CXX=/usr/bin/g++
#export AFL_AS=/usr/bin/as


