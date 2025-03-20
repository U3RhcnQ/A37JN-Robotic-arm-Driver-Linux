# A37JN-Robotic-arm-Driver-Linux
Project for class to build a basic device driver for linux 

Take's in commands trough a charcter device or IOCTL directly to control the device

Commands look as follows: 

- `base:left/right/stop`
- `shoudler:up/down/stop`
- `elbow:up/down/stop`
- `wrist:up/down/stop`
- `claw:open/close/stop`

- `led:on/off`
- `stop:move/all`

For IOCTL you can directly pass 3 int's in a struct do drive the arm.

## Build, Load, and unload
To use the module run the following: ( Note make sure Secure Boot is off )

- `$ make`
- `$ sudo insmod main.ko`
- `$ sudo dmesg` (check if print messages worked)
- `$ sudo rmmod main`
 
