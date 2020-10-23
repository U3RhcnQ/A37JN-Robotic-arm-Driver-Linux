# this is a make file for a kernel object
# see online for more information

# will build "hello.ko"
obj-m += hello.o

# we have no file "hello.c" in this example
# therefore we specify: module hello.ko relies online
# main.c and greet.c ... it's all this makefile module magic
# see online ressources for more information
hello-y := \
	main.o \
	greet.o \

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
