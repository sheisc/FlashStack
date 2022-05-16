export FLASH_STACK_PATH=$(cd $(dirname $0);pwd)

. $FLASH_STACK_PATH/env.sh

export AFL_PATH=$FLASH_STACK_PATH/FlashStack
#export AFL_RUSTC=/home/bdf/.rustup/toolchains/1.43-x86_64-unknown-linux-gnu/bin/rustc
export AFL_RUSTC=`which rustc`
export PATH=$AFL_PATH:$PATH
export LD_PRELOAD=$AFL_PATH/libgsrsp.so


