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

static char device_buffer[BUF_SIZE];

// Structure to hold the active USB device reference
static struct usb_device *active_usb_device = NULL;

static struct usb_device_id usb_ids[] = {
    {USB_DEVICE(0x1267,0x001)},
    {}
};

// global storage for device Major number
static int major = 0;
static int open_count = 0;

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

static struct usb_driver usb_driver = {
    .name = "A37JN Robot arm",
    .id_table = usb_ids,
    .probe = usb_probe,
    .disconnect = usb_disconnect
};

static int send_cmd(unsigned char *ArmCmd) {

    if (!active_usb_device) {
        printk(KERN_ERR "No active USB device\n");
        return -ENODEV;
    }

    printk("Size: %lu\n",sizeof(ArmCmd));
    int ret;

    __u8 bmRequestType = 0x40;
    __u8 bRequest = 6;
    __u16 wValue = 0x100;
    __u16 wIndex = 0;

    ret = usb_control_msg(active_usb_device,
        usb_sndctrlpipe(active_usb_device, 0),
        bRequest,
        bmRequestType,
        wValue, wIndex,
        ArmCmd, sizeof(ArmCmd),
        1000);

    if (ret < 0) {
        printk(KERN_INFO "usb_control_msg failed: %d\n", ret);
    } else {
        printk(KERN_INFO "Sent command to USB device: [%d, %d, %d]\n", ArmCmd[0], ArmCmd[1], ArmCmd[2]);
    }

    return ret;

}

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
    char temp_buf[bytes_read + 1];  // Create buffer with space for null terminator
    memcpy(temp_buf, device_buffer + *offset - bytes_read, bytes_read);
    temp_buf[bytes_read] = '\0';  // Null-terminate


    printk(KERN_INFO "%s: Read %d bytes String: %s\n", KBUILD_MODNAME, bytes_read, temp_buf);
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
    char temp_buf[bytes_write + 1];  // Create buffer with space for null terminator
    memcpy(temp_buf, device_buffer + *offset - bytes_write, bytes_write);
    temp_buf[bytes_write] = '\0';  // Null-terminate

    printk(KERN_INFO "%s: Wrote %d bytes String: %s\n", KBUILD_MODNAME, bytes_write, temp_buf);
    return bytes_write;
}


struct file_operations fops = {
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_close
};

static int __init A37JN_driver_init(void)
{
    printk(KERN_INFO "Loading A37JN Robot arm driver...\n");
    printk(KERN_INFO "Creating Character Device\n");

    // register Character Device
    major = register_chrdev(0, MODULE_NAME, &fops);

    if (major < 0) {
        printk(KERN_ERR "Failed to register Character device: %s with major numer: %d\n",KBUILD_MODNAME ,major);
        return major;
    }

    // Create device class
    char_class = class_create(MODULE_NAME);
    if (IS_ERR(char_class)) {
        unregister_chrdev(major, MODULE_NAME);
        printk(KERN_ALERT "%s: Failed to register device class\n", KBUILD_MODNAME);
        return (int) PTR_ERR(char_class);
    }

    // Create device node automatically
    char_device = device_create(char_class, NULL, MKDEV(major, 0), NULL, MODULE_NAME);
    if (IS_ERR(char_device)) {
        class_destroy(char_class);
        unregister_chrdev(major, MODULE_NAME);
        printk(KERN_ALERT "%s: Failed to create device\n", KBUILD_MODNAME);
        return (int) PTR_ERR(char_device);
    }

    printk(KERN_INFO "Successfully registered Character device: %s with major numer: %d\n",KBUILD_MODNAME ,major);


    printk(KERN_INFO "Registering A37JN Robot arm USB Device\n");
    const int result = usb_register(&usb_driver);
    if (result < 0) {
        printk(KERN_ERR "Failed to register A37JN Robot arm USB Device with Error: %d\n", result);
        return result;
    }
    printk(KERN_INFO "Successfully registered A37JN Robot arm USB Device\n");

    printk(KERN_INFO "Led ON\n");
    unsigned char ArmCmd[3] = {0, 0, 1};  // Array of 3 bytes (example command)
    send_cmd(ArmCmd);

    return 0;
}

static void __exit A37JN_driver_exit(void)
{
    if (major > 0) {
        device_destroy(char_class, MKDEV(major, 0));
        class_unregister(char_class);
        class_destroy(char_class);
        unregister_chrdev(major, MODULE_NAME);
    }
    usb_deregister(&usb_driver);
    printk(KERN_INFO "Goodbye Mr.\n");
}

module_init(A37JN_driver_init);
module_exit(A37JN_driver_exit);

