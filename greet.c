#include <linux/printk.h>
#include "greet.h"

void greet() {
    printk(KERN_INFO "greet(): howdy!\n");
}
