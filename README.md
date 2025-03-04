# Build kernel modules with Clion IDE support

This setup allows Clion IDE to support you with syntax highlightning and
other convenient IDE stuff while developing kernel modules. This works for 
in-tree-development and out-of-tree development (I tested and used both).

Clion doesn't build the kernel module itself, it just supports you while coding.
You have to execute `$ make` on the command line by yourself to build your `.ko`-file!
I personally think that's the better and simpler approach. For another approach
check the original project (link below)!

## Special thanks
Special thanks to the original project: https://gitlab.com/christophacham/cmake-kernel-module
I made some changes to it to fit my needs. And hopefully yours too.

## Attention
- always make sure latest kernel headers are installed `$ sudo apt install kernel-headers-$(uname -r)`
- always synchronize Makefile and CmakeLists.txt (if you add or delete source files)
- check the Clion Cmake log if errors occur. Especially the lines "kernel release"
  and "kernel headers"
- depending on your system you need to change the path where kernel headers
  are stored: `cmake/FindKernelHeaders.cmake # Line 11`

## Build, Load, and unload
- `$ make`
- `$ sudo insmod hello.ko`
- `$ sudo dmesg` (check if print messages worked)
- `$ sudo rmmod hello`
 

## Trivia
I successfully used this Cmake project setup to make changes to existing 
linux drivers. Just clone the linux source, open a specific folder, add
`CMakeLists.txt` and `cmake/`-dir - happy hacking! But be aware of that
you probably don't want to commit cmake-related stuff - at least for the 
release-ready version - Unless you want Linus Torvalds and other kernel
maintainers to beat you.. or so :D
