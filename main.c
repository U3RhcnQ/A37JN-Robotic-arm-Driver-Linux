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
#define BUF_SIZE 256

// global storage for device Major number
static int major = 0;

// Counter for times opened
static int open_count = 0;

// Buffer for character device IO
static char device_buffer[BUF_SIZE];

// Structure to hold the active USB device reference
static struct usb_device *active_usb_device = NULL;

// Table of USB id's
static struct usb_device_id usb_ids[] = {
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
}

// Struct for USB has to be bellow functions, or it breaks ?
static struct usb_driver usb_driver = {
    .name = "A37JN Robot arm",
    .id_table = usb_ids,
    .probe = usb_probe,
    .disconnect = usb_disconnect
};


static int device_open(struct inode *inodep, struct file *filep) {
    open_count++;
    printk(KERN_INFO "%s: Device opened %d times\n", KBUILD_MODNAME, open_count);
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

    if (copy_to_user(buffer, device_buffer + *offset, bytes_read) != 0)
        return -EFAULT;

    *offset += bytes_read;

    // Print the read data (ensure it's null-terminated for safe printing)
    char message[bytes_read + 1];  // Create buffer with space for null terminator
    memcpy(message, device_buffer + *offset - bytes_read, bytes_read);
    message[bytes_read] = '\0';  // Null-terminate

    printk(KERN_INFO "%s: Read %d bytes String: %s", KBUILD_MODNAME, bytes_read, message);
    return bytes_read;
}

static ssize_t device_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    int bytes_write = BUF_SIZE - *offset;
    if (bytes_write > len) bytes_write = len;
    if (bytes_write <= 0) return -ENOMEM;

    if (copy_from_user(device_buffer + *offset, buffer, bytes_write) != 0)
        return -EFAULT;

    *offset += bytes_write;

    // Print the read data (ensure it's null-terminated for safe printing)
    char message[bytes_write + 1];  // Create buffer with space for null terminator
    memcpy(message, device_buffer + *offset - bytes_write, bytes_write);
    message[bytes_write] = '\0';  // Null-terminate

    printk(KERN_INFO "%s: Wrote %d bytes String: %s", KBUILD_MODNAME, bytes_write, message);

    const char *param = strchr(message, ':'); // Find the ':'

    // If ":" exists
    if (!param) {
        printk(KERN_INFO "%s: Invalid command\n", KBUILD_MODNAME);
        return -1;
    }

    const int index = (int) (param - message);
    param++; // move one character forward past:



    if (strncmp(message, "base", index) == 0){
        printk(KERN_INFO "%s: base found", KBUILD_MODNAME);
        if (strcmp(param, "left") == 0) {

        } else if(strcmp(param, "right") == 0) {

        }

    } else if (strncmp(message, "shoulder", index) == 0) {
        printk(KERN_INFO "%s: shoulder found", KBUILD_MODNAME);
    } else if (strncmp(message, "elbow", index) == 0) {
        printk(KERN_INFO "%s: elbow found", KBUILD_MODNAME);
    } else if (strncmp(message, "wrist", index) == 0) {

    } else if (strncmp(message, "gripper", index) == 0) {

    } else if (strncmp(message, "led", index) == 0) {

    } else if (strncmp(message, "stop", index) == 0) {

    }else {
        printk(KERN_INFO "%s: Invalid command\n", KBUILD_MODNAME);
        return -1;
    }

    return bytes_write;
}

struct file_operations fops = {
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_close
};

// Function to send a command to the robot arm
static int send_cmd(unsigned char *ArmCmd) {

    // Sanity Check that USB device exists
    if (!active_usb_device) {
        printk(KERN_ERR "%s: No active USB device\n", KBUILD_MODNAME);
        return -ENODEV;
    }

    printk("Size: %lu\n",sizeof(ArmCmd));
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
        ArmCmd, sizeof(ArmCmd),
        1000);

    if (ret < 0) {
        printk(KERN_INFO "%s: usb_control_msg failed: %d\n", KBUILD_MODNAME, ret);
    } else {
        printk(KERN_INFO "%s: Sent command to USB device: [%d, %d, %d]\n", KBUILD_MODNAME, ArmCmd[0], ArmCmd[1], ArmCmd[2]);
    }

    return ret;

}


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
    unsigned char ArmCmd[3] = {0, 0, 1};  // Array of 3 bytes (example command)
    send_cmd(ArmCmd);


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

