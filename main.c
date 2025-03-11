#include <linux/module.h>     /* Needed by all modules */
#include <linux/kernel.h>     /* Needed for KERN_INFO */
#include <linux/init.h>       /* Needed for the macros */
#include <linux/usb.h>

// The license type
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Petr Pershin");
MODULE_DESCRIPTION("A simple A37JN Robot arm driver");
MODULE_VERSION("1.0");

#define KBUILD_MODNAME "A37JN Robot arm"
#define MODULE_NAME "A37JN_Robot_arm"
#define BUF_SIZE 512

// global storage for device Major number
static int major = 0;

// Need these as they all use the same value to control
static int shoulder_status = 0;
static int elbow_status = 0;
static int wrist_status = 0;
static int gripper_status = 0;

// Command for arm
static int command[3] = {0, 0, 0};

// Buffer for character device IO
static char command_buffer[BUF_SIZE];

// Structure to hold the active USB device reference
static struct usb_device *active_usb_device = NULL;

// Table of USB id's (There can be 2 versions so we account for that)
static struct usb_device_id usb_ids[] = {
    {USB_DEVICE(0x1267,0x000)},
    {USB_DEVICE(0x1267,0x001)},
    {}
};

// Structures for class and device
static struct class *char_class;
static struct device *char_device;

static int usb_probe(struct usb_interface *interface, const struct usb_device_id *id) {
    printk(KERN_INFO "USB device found: Vendor: 0x%04x, Product ID: 0x%04x\n", id->idVendor, id->idProduct);
    active_usb_device = interface_to_usbdev(interface);
    return 0;
}

static void usb_disconnect(struct usb_interface *interface) {
    printk(KERN_INFO "USB device removed\n");
    active_usb_device = NULL;

    // Since device has been disconnected we can reset all values
    command[0] = 0;
    command[1] = 0;
    command[2] = 0;

    shoulder_status = 0;
    elbow_status = 0;
    wrist_status = 0;
    gripper_status = 0;

}

// Struct for USB has to be bellow functions, or it breaks ?
static struct usb_driver usb_driver = {
    .name = "A37JN Robot arm",
    .id_table = usb_ids,
    .probe = usb_probe,
    .disconnect = usb_disconnect
};

void modify_command(const int a, const int b, const int c) {
    command[0] = a;
    command[1] = b;
    command[2] = c;
}

// Function to send a command to the robot arm
static int send_cmd(void) {

    // Sanity Check that USB device exists
    if (!active_usb_device) {
        printk(KERN_ERR "%s: No active USB device\n", KBUILD_MODNAME);
        return -ENODEV;
    }

    unsigned char parsed_command[3];

    // convert list of ints to unsigned char
    // apparently this is the easiest way to do so ?
    // something about the length of the int being unknown (up to four)
    for (int i = 0; i < 3; i++) {
        parsed_command[i] = (unsigned char)command[i];
    }

    printk("Size: %lu\n", sizeof(parsed_command));
    int ret;

    const __u8 bmRequestType = 0x40;
    const __u8 bRequest = 6;
    const __u16 wValue = 0x100;
    const __u16 wIndex = 0;

    ret = usb_control_msg(active_usb_device,
        usb_sndctrlpipe(active_usb_device, 0),
        bRequest,
        bmRequestType,
        wValue, wIndex,
        parsed_command, sizeof(parsed_command),
        1000);

    if (ret < 0) {
        printk(KERN_INFO "%s: usb_control_msg failed: %d\n", KBUILD_MODNAME, ret);
    } else {
        printk(KERN_INFO "%s: Sent command to USB device: [%d, %d, %d] Return: %d \n", KBUILD_MODNAME, parsed_command[0], parsed_command[1], parsed_command[2], ret);
    }

    return ret;

}


static int device_open(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "%s: Device opened\n", KBUILD_MODNAME);
    return 0;
}

static int device_close(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "%s: Device closed\n", KBUILD_MODNAME);
    return 0;
}

static ssize_t device_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    int bytes_read = BUF_SIZE - *offset;
    if (bytes_read > len) bytes_read = len;
    if (bytes_read <= 0) return 0;

    if (copy_to_user(buffer, command_buffer + *offset, bytes_read) != 0)
        return -EFAULT;

    *offset += bytes_read;

    // Print the read data (ensure it's null-terminated for safe printing)
    char message[bytes_read + 1];  // Create buffer with space for null terminator
    memcpy(message, command_buffer + *offset - bytes_read, bytes_read);
    message[bytes_read] = '\0';  // Null-terminate

    printk(KERN_INFO "%s: Read %d bytes String: %s", KBUILD_MODNAME, bytes_read, message);
    return bytes_read;
}

void process_command(const char *input) {

    const char *param = strchr(input, ':'); // Find the ':'

    // If ":" exists
    if (!param) {
        printk(KERN_INFO "%s: Invalid input\n", KBUILD_MODNAME);
        return;
    }

    const int index = (int) (param - input);
    param++; // move one character forward past:

    if (strlen(param) < 2) {
        printk(KERN_INFO "%s: Invalid input\n", KBUILD_MODNAME);
        return;
    }

    if (strncmp(input, "base", index) == 0){
        if (strcmp(param, "left") == 0) {
            printk(KERN_INFO "%s: Turning base left", KBUILD_MODNAME);
            command[1] = 2;
        } else if(strcmp(param, "right") == 0) {
            printk(KERN_INFO "%s: Turning base right", KBUILD_MODNAME);
            command[1] = 1;
        } else if(strcmp(param, "stop") == 0) {
            printk(KERN_INFO "%s: Stopped base", KBUILD_MODNAME);
            command[1] = 0;
        } else {
            printk(KERN_INFO "%s: Invalid base command\n", KBUILD_MODNAME);
        }

    } else if (strncmp(input, "led", index) == 0) {
        if (strcmp(param, "on") == 0) {
            printk(KERN_INFO "%s: Turning led on", KBUILD_MODNAME);
            command[2] = 1;
        } else if(strcmp(param, "off") == 0) {
            printk(KERN_INFO "%s: Turning led off", KBUILD_MODNAME);
            command[2] = 0;
        } else {
            printk(KERN_INFO "%s: Invalid led command\n", KBUILD_MODNAME);
        }

    } else if (strncmp(input, "stop", index) == 0) {
        if (strcmp(param, "move") == 0) {
            printk(KERN_INFO "%s: Stopping movement", KBUILD_MODNAME);
            command[0] = 0;
            command[1] = 0;

            shoulder_status = 0;
            elbow_status = 0;
            wrist_status = 0;
            gripper_status = 0;

        } else if(strcmp(param, "all") == 0) {
            printk(KERN_INFO "%s: Stopping all", KBUILD_MODNAME);
            command[0] = 0;
            command[1] = 0;
            command[2] = 0;

            shoulder_status = 0;
            elbow_status = 0;
            wrist_status = 0;
            gripper_status = 0;

        } else {
            printk(KERN_INFO "%s: Invalid stop command\n", KBUILD_MODNAME);
        }

    } else if (strncmp(input, "shoulder", index) == 0) {
        if (strcmp(param, "up") == 0) {
            printk(KERN_INFO "%s: Turning shoulder up", KBUILD_MODNAME);
            if (shoulder_status == 2) {
                command[0] -= 64;
            }else if (shoulder_status == 0) {
                command[0] += 64;
            }
            shoulder_status = 1;
        } else if(strcmp(param, "down") == 0) {
            printk(KERN_INFO "%s: Turning shoulder down", KBUILD_MODNAME);
            if (shoulder_status == 1) {
                command[0] += 64;
            }else if (shoulder_status == 0) {
                command[0] += 128;
            }
            shoulder_status = 2;
        } else if(strcmp(param, "stop") == 0) {
            printk(KERN_INFO "%s: Stopped shoulder", KBUILD_MODNAME);
            if (shoulder_status == 1) {
                command[0] -= 64;
            } else if (shoulder_status == 2) {
                command[0] -= 128;
            }
            shoulder_status = 0;
        } else {
            printk(KERN_INFO "%s: Invalid shoulder command\n", KBUILD_MODNAME);
        }

    } else if (strncmp(input, "elbow", index) == 0) {
        if (strcmp(param, "up") == 0) {
            printk(KERN_INFO "%s: Turning elbow up", KBUILD_MODNAME);
            if (elbow_status == 2) {
                command[0] -= 16;
            }else if (elbow_status == 0) {
                command[0] += 16;
            }
            elbow_status = 1;
        } else if(strcmp(param, "down") == 0) {
            printk(KERN_INFO "%s: Turning elbow down", KBUILD_MODNAME);
            if (elbow_status == 1) {
                command[0] += 16;
            }else if (elbow_status == 0) {
                command[0] += 32;
            }
            elbow_status = 2;
        } else if(strcmp(param, "stop") == 0) {
            printk(KERN_INFO "%s: Stopped elbow", KBUILD_MODNAME);
            if (elbow_status == 1) {
                command[0] -= 16;
            } else if (elbow_status == 2) {
                command[0] -= 32;
            }
            elbow_status = 0;
        } else {
            printk(KERN_INFO "%s: Invalid elbow command\n", KBUILD_MODNAME);
        }

    } else if (strncmp(input, "wrist", index) == 0) {
        if (strcmp(param, "up") == 0) {
            printk(KERN_INFO "%s: Turning wrist up", KBUILD_MODNAME);
            if (wrist_status == 2) {
                command[0] -= 4;
            }else if (wrist_status == 0) {
                command[0] += 4;
            }
            wrist_status = 1;
        } else if(strcmp(param, "down") == 0) {
            printk(KERN_INFO "%s: Turning wrist down", KBUILD_MODNAME);
            if (wrist_status == 1) {
                command[0] += 4;
            }else if (wrist_status == 0) {
                command[0] += 8;
            }
            wrist_status = 2;
        } else if(strcmp(param, "stop") == 0) {
            printk(KERN_INFO "%s: Stopped wrist", KBUILD_MODNAME);
            if (wrist_status == 1) {
                command[0] -= 4;
            } else if (wrist_status == 2) {
                command[0] -= 8;
            }
            wrist_status = 0;
        } else {
            printk(KERN_INFO "%s: Invalid wrist command\n", KBUILD_MODNAME);
        }

    } else if (strncmp(input, "gripper", index) == 0) {
        if (strcmp(param, "up") == 0) {
            printk(KERN_INFO "%s: Turning gripper up", KBUILD_MODNAME);
            if (gripper_status == 2) {
                command[0] -= 1;
            }else if (gripper_status == 0) {
                command[0] += 1;
            }
            gripper_status = 1;
        } else if(strcmp(param, "down") == 0) {
            printk(KERN_INFO "%s: Turning gripper down", KBUILD_MODNAME);
            if (gripper_status == 1) {
                command[0] += 1;
            }else if (gripper_status == 0) {
                command[0] += 2;
            }
            gripper_status = 2;
        } else if(strcmp(param, "stop") == 0) {
            printk(KERN_INFO "%s: Stopped gripper", KBUILD_MODNAME);
            if (gripper_status == 1) {
                command[0] -= 1;
            } else if (gripper_status == 2) {
                command[0] -= 2;
            }
            gripper_status = 0;
        } else {
            printk(KERN_INFO "%s: Invalid gripper command\n", KBUILD_MODNAME);
        }

    } else {
        printk(KERN_INFO "%s: Invalid command\n", KBUILD_MODNAME);
    }
}


static ssize_t device_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    if (len > BUF_SIZE - 1) {
        printk(KERN_INFO "%s: Command buffer overflow!", KBUILD_MODNAME);
        return -ENOMEM; // Not enough space
    }

    if (copy_from_user(command_buffer, buffer, len) != 0) {
        return -EFAULT;
    }

    command_buffer[len] = '\0';  // Ensure null termination

    // Print the written data
    printk(KERN_INFO "%s: Wrote %lu bytes String: %s", KBUILD_MODNAME, len, command_buffer);

    // Loop through the buffer to process all commands
    char *cmd_start = command_buffer;
    char *cmd_end;

    while ((cmd_end = strchr(cmd_start, '\n')) != NULL) {

        printk(KERN_INFO "%s: Processing: %s", KBUILD_MODNAME, cmd_start);

        *cmd_end = '\0'; // Null terminate the current command
        process_command(cmd_start); // Process the command

        // Move cmd_start to the next command after the newline
        cmd_start = cmd_end + 1;
    }

    // If there's any leftover data (no newline at the end), handle it as the last command
    if (*cmd_start != '\0') {
        process_command(cmd_start);
    }

    // Send processed command to robot arm
    send_cmd();

    // Clear the buffer so it does not hold stale data
    memset(command_buffer, 0, BUF_SIZE);

    return len;
}

struct file_operations fops = {
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_close
};


// Initialisation logic
static int __init A37JN_driver_init(void){

    printk(KERN_INFO "%s: Loading A37JN Robot arm driver...\n",KBUILD_MODNAME);
    printk(KERN_INFO "%s: Creating Character Device\n",KBUILD_MODNAME);

    // Register Character Device
    major = register_chrdev(0, MODULE_NAME, &fops);

    // We check if we registered successfully
    if (major < 0) {
        printk(KERN_ERR "%s: Failed to register Character device: %s with major numer: %d\n",KBUILD_MODNAME ,KBUILD_MODNAME ,major);
        return major;
    }

    // Create device class
    char_class = class_create(MODULE_NAME);
    if (IS_ERR(char_class)) {
        unregister_chrdev(major, MODULE_NAME);
        printk(KERN_ALERT "%s: Failed to register device class\n", KBUILD_MODNAME);
        return (int) PTR_ERR(char_class);
    }

    // Create device node
    char_device = device_create(char_class, NULL, MKDEV(major, 0), NULL, MODULE_NAME);
    if (IS_ERR(char_device)) {
        class_destroy(char_class);
        unregister_chrdev(major, MODULE_NAME);
        printk(KERN_ALERT "%s: Failed to create device\n", KBUILD_MODNAME);
        return (int) PTR_ERR(char_device);
    }

    printk(KERN_INFO "%s: Successfully registered Character device with major numer: %d\n", KBUILD_MODNAME, major);
    printk(KERN_INFO "%s: Registering A37JN Robot arm USB Device\n",KBUILD_MODNAME);

    const int result = usb_register(&usb_driver);
    if (result < 0) {
        printk(KERN_ERR "%s: Failed to register A37JN Robot arm USB Device with Error: %d\n",KBUILD_MODNAME, result);
        return result;
    }

    printk(KERN_INFO "%s: Successfully registered A37JN Robot arm USB Device\n",KBUILD_MODNAME);


    printk(KERN_INFO "%s: Led ON\n",KBUILD_MODNAME);
    modify_command(0,0,1); // Array of 3 bytes (example command)
    send_cmd();


    return 0;
}

static void __exit A37JN_driver_exit(void){

    // we need to check if stuff has been initialised before destroying
    if (major > 0) {
        device_destroy(char_class, MKDEV(major, 0));
        class_unregister(char_class);
        class_destroy(char_class);
        unregister_chrdev(major, MODULE_NAME);
    }

    usb_deregister(&usb_driver);
    printk(KERN_INFO "%s: Goodbye Kernel\n",KBUILD_MODNAME);

}

module_init(A37JN_driver_init);
module_exit(A37JN_driver_exit);

