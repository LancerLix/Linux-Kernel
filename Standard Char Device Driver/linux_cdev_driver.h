#ifndef _STANDARD_CHAR_DEV_H_
#define _STANDARD_CHAR_DEV_H_

#include <linux/cdev.h>
#include <linux/semaphore.h>

#define MY_DEVICE_NODE_NAME  "lix_dev"
#define MY_DEVICE_FILE_NAME  "lix_dev"
#define MY_DEVICE_PROC_NAME  "lix_dev_proc"
#define MY_DEVICE_CLASS_NAME "lix_dev_class"

struct lix_dev {
    int val;
    struct semaphore sem;
    struct cdev dev;
};

#endif