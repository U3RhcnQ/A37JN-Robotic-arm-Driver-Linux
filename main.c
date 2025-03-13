#include <linux/module.h>
#include <linux/kernel.h> // Needed for KERN_INFO
#include <linux/init.h>  // Needed for the macros
#include <linux/usb.h>  // Needed for usb
#include <linux/proc_fs.h> // Proc file stuff
#include <linux/seq_file.h>


// The license type
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Petr Pershin");
MODULE_DESCRIPTION("A simple A37JN Robot arm driver");
MODULE_VERSION("1.0");

#define KBUILD_MODNAME "A37JN Robot arm"
#define MODULE_NAME "A37JN_Robot_arm"
#define BUF_SIZE 512

#define MAGIC_NUM 0x80
#define IOCTL_SET_VALUE _IOW(MAGIC_NUM, 1, int)
#define IOCTL_GET_VALUE _IOR(MAGIC_NUM, 2, int)


// global storage for device Major number
static int major = 0;

// Need these as they all use the same value to control
static int shoulder_status = 0;
static int elbow_status = 0;
static int wrist_status = 0;
static int claw_status = 0;

static int connection_status = 0;
static int command_status = 0;
static int battery_level = 0;

// Text for nice print
const char *connection_status_text;
const char *command_status_text;

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

// Helper to quicly modify the whole command
static void modify_command(const int a, const int b, const int c) {
    command[0] = a;
    command[1] = b;
    command[2] = c;
}

static int usb_probe(struct usb_interface *interface, const struct usb_device_id *id) {
    printk(KERN_INFO "%s: USB device found: Vendor: 0x%04x, Product ID: 0x%04x\n", KBUILD_MODNAME, id->idVendor, id->idProduct);
    active_usb_device = interface_to_usbdev(interface);
    connection_status = 1;
    return 0;
}

static void usb_disconnect(struct usb_interface *interface) {
    printk(KERN_INFO "%s: USB device removed\n", KBUILD_MODNAME);
    active_usb_device = NULL;
    connection_status = 0;

    // Since device has been disconnected we can reset all values
    modify_command(0, 0, 0);

    shoulder_status = 0;
    elbow_status = 0;
    wrist_status = 0;
    claw_status = 0;
}

// Struct for USB has to be bellow functions, or it breaks ?
static struct usb_driver usb_driver = {
    .name = "A37JN Robot arm",
    .id_table = usb_ids,
    .probe = usb_probe,
    .disconnect = usb_disconnect
};


// Function to send a command to the robot arm
static int send_cmd(void) {

    // Sanity Check that USB device exists
    if (!active_usb_device) {
        printk(KERN_ERR "%s: No active USB device\n", KBUILD_MODNAME);
        connection_status = 0;
        return -ENODEV;
    }

    unsigned char parsed_command[3];

    // convert list of ints to unsigned char
    // apparently this is the easiest way to do so ?
    // something about the length of the int being unknown (up to four)
    for (int i = 0; i < 3; i++) {
        parsed_command[i] = (unsigned char)command[i];
    }

    //printk("Size: %lu\n", sizeof(parsed_command));
    int ret;

    const __u8 bmRequestType = 0x40;
    const __u8 bRequest = 6;
    const __u16 wValue = 0x100;
    const __u16 wIndex = 0;

    // Send control message to usb
    ret = usb_control_msg(active_usb_device,
        usb_sndctrlpipe(active_usb_device, 0),
        bRequest,
        bmRequestType,
        wValue, wIndex,
        parsed_command, sizeof(parsed_command),
        1000);

    if (ret < 0) {
        printk(KERN_INFO "%s: USB control message failed with code: %d\n", KBUILD_MODNAME, ret);
        battery_level = 0;
        connection_status = 0;
    } else {
        printk(KERN_INFO "%s: Sent command to USB device: [%d, %d, %d] Return: %d \n", KBUILD_MODNAME, parsed_command[0], parsed_command[1], parsed_command[2], ret);
        battery_level = ret;
        connection_status = 1;
    }

    return ret;
}

static int device_open(struct inode *inode_pointer, struct file *file_pointer) {
    printk(KERN_INFO "%s: Device opened\n", KBUILD_MODNAME);
    return 0;
}

static int device_close(struct inode *inode_pointer, struct file *file_pointer) {
    printk(KERN_INFO "%s: Device closed\n", KBUILD_MODNAME);
    return 0;
}

static void update_status_text(void) {
    if (connection_status == 1) {
        connection_status_text = "yes";
    } else {
        // Assume we are not connected
        connection_status_text = "no";
        command_status = 0;
        battery_level = 0;
    }

    if (command_status == 1) {
        command_status_text = "good";
    } else if (command_status == 2) {
        command_status_text = "bad";
    } else {
        command_status_text = "none";
    }
}

static ssize_t device_read(struct file *file_pointer, char __user *buffer, size_t len, loff_t *offset) {

    char status_message[64];
    int msg_length;

    // Update status text
    update_status_text();

    // Format the message
    msg_length = snprintf(status_message, sizeof(status_message), "connected:%s status:%s battery:%d\n", connection_status_text, command_status_text, battery_level);

    // Handle the offset (ensures the message is only read once per call)
    if (*offset >= msg_length) {
        return 0; // EOF
    }

    // Copy message to user space
    if (copy_to_user(buffer, status_message, msg_length)) {
        return -EFAULT;
    }

    *offset += msg_length; // Update file offset
    return msg_length;
}

static void process_command(const char *input) {

    const char *param = strchr(input, ':'); // Find the ':'

    // If ":" exists
    if (!param) {
        printk(KERN_INFO "%s: Invalid input\n", KBUILD_MODNAME);
        command_status = 2;
        return;
    }

    const int index = (int) (param - input);
    param++; // move one character forward past:

    if (strlen(param) < 2) {
        printk(KERN_INFO "%s: Invalid input\n", KBUILD_MODNAME);
        command_status = 2;
        return;
    }

    if (strncmp(input, "base", index) == 0){
        if (strcmp(param, "left") == 0) {
            printk(KERN_INFO "%s: Turning base left", KBUILD_MODNAME);
            command[1] = 2;
            command_status = 1;
        } else if(strcmp(param, "right") == 0) {
            printk(KERN_INFO "%s: Turning base right", KBUILD_MODNAME);
            command[1] = 1;
            command_status = 1;
        } else if(strcmp(param, "stop") == 0) {
            printk(KERN_INFO "%s: Stopped base", KBUILD_MODNAME);
            command[1] = 0;
            command_status = 1;
        } else {
            printk(KERN_INFO "%s: Invalid base command\n", KBUILD_MODNAME);
            command_status = 2;
        }

    } else if (strncmp(input, "led", index) == 0) {
        if (strcmp(param, "on") == 0) {
            printk(KERN_INFO "%s: Turning led on", KBUILD_MODNAME);
            command[2] = 1;
            command_status = 1;
        } else if(strcmp(param, "off") == 0) {
            printk(KERN_INFO "%s: Turning led off", KBUILD_MODNAME);
            command[2] = 0;
            command_status = 1;
        } else {
            printk(KERN_INFO "%s: Invalid led command\n", KBUILD_MODNAME);
            command_status = 2;
        }

    } else if (strncmp(input, "stop", index) == 0) {
        if (strcmp(param, "move") == 0) {
            printk(KERN_INFO "%s: Stopping movement", KBUILD_MODNAME);
            command[0] = 0;
            command[1] = 0;
            command_status = 1;

            shoulder_status = 0;
            elbow_status = 0;
            wrist_status = 0;
            claw_status = 0;

        } else if(strcmp(param, "all") == 0) {
            printk(KERN_INFO "%s: Stopping all", KBUILD_MODNAME);

            modify_command(0,0,0);
            command_status = 1;

            shoulder_status = 0;
            elbow_status = 0;
            wrist_status = 0;
            claw_status = 0;

        } else {
            printk(KERN_INFO "%s: Invalid stop command\n", KBUILD_MODNAME);
            command_status = 2;
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
            command_status = 1;
        } else if(strcmp(param, "down") == 0) {
            printk(KERN_INFO "%s: Turning shoulder down", KBUILD_MODNAME);
            if (shoulder_status == 1) {
                command[0] += 64;
            }else if (shoulder_status == 0) {
                command[0] += 128;
            }
            shoulder_status = 2;
            command_status = 1;
        } else if(strcmp(param, "stop") == 0) {
            printk(KERN_INFO "%s: Stopped shoulder", KBUILD_MODNAME);
            if (shoulder_status == 1) {
                command[0] -= 64;
            } else if (shoulder_status == 2) {
                command[0] -= 128;
            }
            shoulder_status = 0;
            command_status = 1;
        } else {
            printk(KERN_INFO "%s: Invalid shoulder command\n", KBUILD_MODNAME);
            command_status = 2;
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
            command_status = 1;
        } else if(strcmp(param, "down") == 0) {
            printk(KERN_INFO "%s: Turning elbow down", KBUILD_MODNAME);
            if (elbow_status == 1) {
                command[0] += 16;
            }else if (elbow_status == 0) {
                command[0] += 32;
            }
            elbow_status = 2;
            command_status = 1;
        } else if(strcmp(param, "stop") == 0) {
            printk(KERN_INFO "%s: Stopped elbow", KBUILD_MODNAME);
            if (elbow_status == 1) {
                command[0] -= 16;
            } else if (elbow_status == 2) {
                command[0] -= 32;
            }
            elbow_status = 0;
            command_status = 1;
        } else {
            printk(KERN_INFO "%s: Invalid elbow command\n", KBUILD_MODNAME);
            command_status = 2;
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
            command_status = 1;
        } else if(strcmp(param, "down") == 0) {
            printk(KERN_INFO "%s: Turning wrist down", KBUILD_MODNAME);
            if (wrist_status == 1) {
                command[0] += 4;
            }else if (wrist_status == 0) {
                command[0] += 8;
            }
            wrist_status = 2;
            command_status = 1;
        } else if(strcmp(param, "stop") == 0) {
            printk(KERN_INFO "%s: Stopped wrist", KBUILD_MODNAME);
            if (wrist_status == 1) {
                command[0] -= 4;
            } else if (wrist_status == 2) {
                command[0] -= 8;
            }
            wrist_status = 0;
            command_status = 1;
        } else {
            printk(KERN_INFO "%s: Invalid wrist command\n", KBUILD_MODNAME);
            command_status = 2;
        }

    } else if (strncmp(input, "claw", index) == 0) {
        if (strcmp(param, "close") == 0) {
            printk(KERN_INFO "%s: Closing claw", KBUILD_MODNAME);
            if (claw_status == 2) {
                command[0] -= 1;
            }else if (claw_status == 0) {
                command[0] += 1;
            }
            claw_status = 1;
            command_status = 1;
        } else if(strcmp(param, "open") == 0) {
            printk(KERN_INFO "%s: Opening claw", KBUILD_MODNAME);
            if (claw_status == 1) {
                command[0] += 1;
            }else if (claw_status == 0) {
                command[0] += 2;
            }
            claw_status = 2;
            command_status = 1;
        } else if(strcmp(param, "stop") == 0) {
            printk(KERN_INFO "%s: Stopped claw", KBUILD_MODNAME);
            if (claw_status == 1) {
                command[0] -= 1;
            } else if (claw_status == 2) {
                command[0] -= 2;
            }
            claw_status = 0;
            command_status = 1;
        } else {
            printk(KERN_INFO "%s: Invalid claw command\n", KBUILD_MODNAME);
            command_status = 2;
        }

    } else {
        printk(KERN_INFO "%s: Invalid command\n", KBUILD_MODNAME);
        command_status = 2;
    }
}


static ssize_t device_write(struct file *file_pointer, const char *buffer, const size_t len, loff_t *offset) {
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

static long device_ioctl(struct file *file, const unsigned int cmd, const unsigned long arg) {

    // Only way to pass multiple ints via ioctl so we have to use a struct
    struct device_command {
        int var1;
        int var2;
        int var3;
    };

    struct device_command command;

    switch (cmd) {
        case IOCTL_SET_VALUE:
            
        if (copy_from_user(&command, (struct device_command __user *)arg, sizeof(struct device_command))) {
            return -EFAULT;
        }

        // A bit messy but it works :/
        if (command.var1 < 0 || command.var2 < 0 || command.var3 < 0 ||
            command.var1 % 2 !=0 || command.var2 % 2 !=0 || command.var3 % 2 !=0 ||
            command.var1 <= 170 || command.var2 <= 2 || command.var3 <= 2) {
            return -EINVAL; // Reject invalid values
        }

        printk(KERN_INFO "%s: Direct control values: %d,%d,%d\n", KBUILD_MODNAME, command.var1, command.var2, command.var3);

        // Execute the command
        modify_command(command.var1, command.var2, command.var3);
        command_status = 1;

        // We need to set all the statuses so we remain in sync
        if (command.var1 >= 128) {
            shoulder_status = 2;
            command.var1 -= 128;
        } else if (command.var1 >= 64) {
            shoulder_status = 1;
            command.var1 -= 64;
        } else {
            shoulder_status = 0;
        }

        if (command.var1 >= 32) {
            elbow_status = 2;
            command.var1 -= 32;
        } else if (command.var1 >= 16) {
            elbow_status = 1;
            command.var1 -= 16;
        } else {
            elbow_status = 0;
        }

        if (command.var1 >= 8) {
            wrist_status = 2;
            command.var1 -= 8;
        } else if (command.var1 >= 4) {
            wrist_status = 1;
            command.var1 -= 4;
        } else {
            wrist_status = 0;
        }

        if (command.var1 >= 2) {
            claw_status = 1;
        } else if (command.var1 >= 1) {
            claw_status = 2;
        } else {
            claw_status = 0;
        }

        break;

    default:
        return -EINVAL;  // Invalid command
        
    }

    return 0;
}

struct file_operations fops = {
    .unlocked_ioctl = device_ioctl,
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_close
};

static int proc_show(struct seq_file *m, void *v) {

    // Update status text
    update_status_text();

    seq_printf(m, "Shoulder Status: %d\n", shoulder_status);
    seq_printf(m, "Elbow Status: %d\n", elbow_status);
    seq_printf(m, "Wrist Status: %d\n", wrist_status);
    seq_printf(m, "Claw Status: %d\n", claw_status);
    seq_printf(m, "connected:%s status:%s battery:%d\n", connection_status_text, command_status_text, battery_level);

    return 0;
}

static int proc_open(struct inode *inode, struct file *file) {
    return single_open(file, proc_show, NULL);
}

static const struct proc_ops proc_fops = {
    .proc_open    = proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
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

        // Bail if we cannot make a device class
        unregister_chrdev(major, MODULE_NAME);
        printk(KERN_ERR "%s: Failed to register device class\n", KBUILD_MODNAME);
        return (int) PTR_ERR(char_class);
    }

    // Create device node
    char_device = device_create(char_class, NULL, MKDEV(major, 0), NULL, MODULE_NAME);
    if (IS_ERR(char_device)) {

        // Bail if we cannot make a device node
        class_destroy(char_class);
        unregister_chrdev(major, MODULE_NAME);
        printk(KERN_ERR "%s: Failed to create device\n", KBUILD_MODNAME);
        return (int) PTR_ERR(char_device);
    }

    printk(KERN_INFO "%s: Successfully registered Character device with major numer: %d\n", KBUILD_MODNAME, major);
    printk(KERN_INFO "%s: Registering A37JN Robot arm USB Device\n",KBUILD_MODNAME);

    const int result = usb_register(&usb_driver);
    if (result < 0) {

        // Bail if we cannot register device
        device_destroy(char_class, MKDEV(major, 0));
        class_destroy(char_class);
        unregister_chrdev(major, MODULE_NAME);
        
        printk(KERN_ERR "%s: Failed to register A37JN Robot arm USB Device with Error: %d\n",KBUILD_MODNAME, result);
        return result;
    }

    struct proc_dir_entry *proc_entry;

    proc_entry = proc_create(KBUILD_MODNAME, 0444, NULL, &proc_fops);
    if (!proc_entry) {
        printk(KERN_ERR "%s: Failed to create Proc file\n", KBUILD_MODNAME);

        // Bail if we cannot register proc file
        usb_deregister(&usb_driver);
        device_destroy(char_class, MKDEV(major, 0));
        class_destroy(char_class);
        unregister_chrdev(major, MODULE_NAME);

        return -ENOMEM;
    }
    printk(KERN_INFO "%s: Proc file Created successfully \n", KBUILD_MODNAME);


    printk(KERN_INFO "%s: Successfully registered A37JN Robot arm USB Device\n",KBUILD_MODNAME);

    // Testing
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

