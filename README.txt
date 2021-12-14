# RFG++: Practical Software-Based Parallel Shadow Stacks on x86-64



## How to Build Nginx

(1) Normal Build and Get the Function Names

iron@CSE:nginx-1.18.0$ CC="spa-clang -O3" ./configure --prefix=$(pwd)/bin

iron@CSE:nginx-1.18.0$ make -j4 2>&1 | tee ~/nginx.build.txt

iron@CSE:nginx-1.18.0$ cat ~/nginx.build.txt | grep "###SPA_FUNCNAME###" | awk '{printf $2"\n"}' | uniq | sort > /home/iron/nginx.funcnames.txt

(2) Once we get the names of the protected functions, we can reuse these names and rebuild Nginx to protect its calls from TOCTTOU attacks.

iron@CSE:nginx-1.18.0$ export __SPA_PROTECTED_FUNCS_PATH=/home/iron/nginx.funcnames.txt
iron@CSE:nginx-1.18.0$ make clean
iron@CSE:nginx-1.18.0$ make -j4

## Function Names for CPU2006, Firefox, HTTPD, and Nginx

We have provided the following files for CPU2006, Firefox79.0, httpd2.4.46 and nginx1.18 in the source code directory

cpu2006.protected.funcs.txt
firefox79.0.protected.funcs.txt
httpd2.4.46.protected_funcs.txt
nginx1.18.protected_funcs.txt

## Build Firefox79.0

To build Firefox79.0, please use the two files WasmSignalHandlers.cpp and SandboxFilterUtil.cpp 
provided to replace the one in the Firefox79.0.

The mozconfig is configured as follows:

mk_add_options MOZ_OBJDIR=$topsrcdir/obj.flashstack
mk_add_options MOZ_MAKE_FLAGS="-j4"
ac_add_options --enable-application=browser
ac_add_options CC="spa-clang"
ac_add_options CXX="spa-clang++"
ac_add_options --prefix=$topsrcdir/install.flashstack
ac_add_options --disable-debug-symbols


iron@CSE:~$ cd /home/iron/Downloads/firefox-79.0.source/firefox-79.0
iron@CSE:firefox-79.0$ . ~/apps/FlashStack.sh 

iron@CSE:firefox-79.0$ ./mach build


## Download the VM for artifact evaluation

The password is 123456

https://drive.google.com/file/d/1H1BmYNbAP08QyD_Worm4RXLqWE_DnHLt/view

