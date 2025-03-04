# A37JN-Robotic-arm-Driver-Linux
Project for class to build a basic device driver for linux 

## Build, Load, and unload
To use the module run the following: ( Note make sure Secure Boot is off )

- `$ make`
- `$ sudo insmod main.ko`
- `$ sudo dmesg` (check if print messages worked)
- `$ sudo rmmod main`
 


