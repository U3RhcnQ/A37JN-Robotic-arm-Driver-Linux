#include <linux/module.h>     /* Needed by all modules */
#include <linux/kernel.h>     /* Needed for KERN_INFO */
#include <linux/init.h>       /* Needed for the macros */
#include <linux/usb.h>

// The license type
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bob");
MODULE_DESCRIPTION("A simple Hello world LKM!");
MODULE_VERSION("1.0");

#define KBUILD_MODNAME "A37JN Robot arm"

// Structure to hold the active USB device reference
static struct usb_device *active_usb_device = NULL;

static struct usb_device_id usb_ids[] = {
    {USB_DEVICE(0x1267,0x001)},
    {}
};

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
    .disconnect = usb_disconnect,
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


static int __init A37JN_driver_init(void)
{
    printk(KERN_INFO "Loading hello module...\n");
    printk(KERN_INFO "Hello world\n");

    int result = usb_register(&usb_driver);
    if (result < 0) {
        printk(KERN_ERR "usb_register failed. Error: %d\n", result);
        return result;
    }

    printk(KERN_INFO "Led ON\n");
    unsigned char ArmCmd[3] = {0, 0, 1};  // Array of 3 bytes (example command)
    send_cmd(ArmCmd);

    return 0;
}

static void __exit A37JN_driver_exit(void)
{
    usb_deregister(&usb_driver);
    printk(KERN_INFO "Goodbye Mr.\n");
}

module_init(A37JN_driver_init);
module_exit(A37JN_driver_exit);

