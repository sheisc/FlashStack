### 1. Build SPA

Open a terminal

cd ~/src/SPA
make


### 2. Use SPA to build CPU2006

(1) set the environment variables in ~/src/SPA/SPA.LLVM7.0.sh

export AFL_PATH=/home/iron/src/SPA
export LLVM_INSTALL_PATH=/home/iron/src/SP2021/llvm-7.0.0

(2) set the environment variables in ~/src/cpu2006/config/cpu2006.cfg
CC           = afl-clang -std=gnu89 -D_GNU_SOURCE
CXX          = afl-clang++ -std=c++03  -D_GNU_SOURCE 

(3)  

cd ~/src/cpu2006/
. ~/src/SPA/SPA.LLVM7.0.sh
. ./shrc

ulimit -s 262144


rm -rf benchspec/CPU2006/*/exe/
runspec  --action=clean --config=cpu2006 all
runspec  --action=build --config=cpu2006 401 403 445 464 456 470 462 429 433 400 458 482 473 447 444 471 453 450 483
runspec  --action=run --config=cpu2006 --tune=base  --iterations=5 401 403 445 464 456 470 462 429 433 400 458 482 473 447 444 471 453 450 483


###  3. Use SPA to build Firefox-79

Open a new terminal

cp ~/src/SPA/mozconfig ~/src/firefox-79.0/

. ~/src/SPA/SPA.LLVM7.0.sh
ulimit -s 262144
export LD_PRELOAD=/home/iron/src/SPA/fork.so

cd ~/src/firefox-79.0
./mach build 2>&1 | tee build.firefox.log.txt


