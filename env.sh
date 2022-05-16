export LC_ALL=C

export FLASH_STACK_PATH=$(cd $(dirname $0);pwd)

# LLVM environments
export LLVM_INSTALL_PATH=$FLASH_STACK_PATH/llvm-7.0.0
export PATH=$LLVM_INSTALL_PATH/bin:$PATH
export LLVM_COMPILER=clang
export LLVM_DIR=$LLVM_INSTALL_PATH
export LD_LIBRARY_PATH=$LLVM_INSTALL_PATH/lib/



