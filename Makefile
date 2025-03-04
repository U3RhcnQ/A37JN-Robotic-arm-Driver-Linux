# this is a make file for a kernel object
# see online for more information

# will build "main.ko"
obj-m += main.o

# Define the output directory
OUT_DIR := $(PWD)/out/

all:
	mkdir -p $(OUT_DIR)
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD)  modules
	mv .*.*.cmd *.o *.ko *.mod *.mod.c Module.symvers modules.order $(OUT_DIR)/

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -rf $(OUT_DIR)/*