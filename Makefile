# this is a make file for a kernel object
# see online for more information

# will build "hello.ko"
obj-m += hello.o

# we have no file "hello.c" in this example
# therefore we specify: module hello.ko relies on
# main.c and greet.c ... it's this makefile module magic thing..
# see online resources for more information
# YOU DON'T need this IF you have *.c-file with the name of the
# final kernel module :)
hello-y := \
	main.o \
	greet.o \

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
