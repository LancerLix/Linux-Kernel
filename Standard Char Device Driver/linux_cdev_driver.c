#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/version.h>
#include <asm/uaccess.h>

#include "linux_cdev_driver.h"

#define PROC_ONLY_FOR_READ 0

/*主设备和从设备号变量*/
static int my_dev_major = 0;
static int my_dev_minor = 0;

/*设备类别和设备变量*/
static struct class* my_dev_class = NULL;
static struct lix_dev* my_dev = NULL;

/*传统的设备文件操作方法*/
static int my_dev_open(struct inode* inode, struct file* filp);
static int my_dev_release(struct inode* inode, struct file* filp);
static ssize_t my_dev_read(struct file* filp, char __user *buf, size_t count, loff_t* f_pos);
static ssize_t my_dev_write(struct file* filp, const char __user *buf, size_t count, loff_t* f_pos);

/*设备文件操作方法表*/
static struct file_operations my_dev_fops = {
	.owner = THIS_MODULE,
	.open = my_dev_open,
	.release = my_dev_release,
	.read = my_dev_read,
	.write = my_dev_write,
};

/*打开设备方法*/
static int my_dev_open(struct inode* inode, struct file* filp)
{
	struct lix_dev* dev;
	printk("Call %s line %d\n",__FUNCTION__,__LINE__);
	
	/*获取dev_set_drvdata保存的自定义数据设备结构体*/
	dev = container_of(inode->i_cdev, struct lix_dev, dev);
	/*将自定义数据设备结构体保存在file指针的私有数据域中，供其他获取到file指针的地方获取*/
	filp->private_data = dev;

	return 0;
}

/*设备文件释放时调用，空实现*/
static int my_dev_release(struct inode* inode, struct file* filp)
{
	printk("Call %s line %d\n",__FUNCTION__,__LINE__);
	return 0;
}

/*读取设备的寄存器val的值*/
static ssize_t my_dev_read(struct file* filp, char __user *buf, size_t count, loff_t* f_pos)
{
	ssize_t err = 0;
	struct lix_dev* dev = filp->private_data;
	printk("Call %s line %d\n",__FUNCTION__,__LINE__);

	/*同步访问*/
	if(down_interruptible(&(dev->sem))) {
		return -ERESTARTSYS;
	}

	if(count < sizeof(dev->val)) {
		goto out;
	}

	/*将寄存器val的值拷贝到用户提供的缓冲区*/
	if(copy_to_user(buf, &(dev->val), sizeof(dev->val))) {
		err = -EFAULT;
		goto out;
	}

	err = sizeof(dev->val);

out:
	up(&(dev->sem));
	return err;
}

/*写设备的寄存器值val*/
static ssize_t my_dev_write(struct file* filp, const char __user *buf, size_t count, loff_t* f_pos)
{
	ssize_t err = 0;
	struct lix_dev* dev = filp->private_data;
	printk("Call %s line %d\n",__FUNCTION__,__LINE__);

	/*同步访问*/
	if(down_interruptible(&(dev->sem))) {
		return -ERESTARTSYS;
	}

	if(count != sizeof(dev->val)) {
		goto out;
	}

	/*将用户提供的缓冲区的值写到设备寄存器去*/
	if(copy_from_user(&(dev->val), buf, count)) {
		err = -EFAULT;
		goto out;
	}

	err = sizeof(dev->val);

out:
	up(&(dev->sem));
	return err;
}

/*访问设置属性方法*/
static ssize_t my_attr_show(struct device* dev, struct device_attribute* attr,  char* buf);
static ssize_t my_attr_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);

/*读取寄存器val的值到缓冲区buf中，内部使用*/
static ssize_t get_val(struct lix_dev* dev, char* buf)
{
	int val = 0;

	/*同步访问*/
	if(down_interruptible(&(dev->sem))) {
		return -ERESTARTSYS;
	}

	val = dev->val;
	up(&(dev->sem));

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

/*把缓冲区buf的值写到设备寄存器val中去，内部使用*/
static ssize_t set_val(struct lix_dev* dev, const char* buf, size_t count)
{
	int val = 0;

	/*将字符串转换成数字*/
	val = simple_strtol(buf, NULL, 10);

	/*同步访问*/
	if(down_interruptible(&(dev->sem))) {
		return -ERESTARTSYS;
	}

	dev->val = val;
	up(&(dev->sem));

	return count;
}

/*读取设备属性val*/
static ssize_t my_attr_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	struct lix_dev* ldev = (struct lix_dev*)dev_get_drvdata(dev);
	printk("Call %s line %d\n",__FUNCTION__,__LINE__);

	return get_val(ldev, buf);
}

/*写设备属性val*/
static ssize_t my_attr_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
	struct lix_dev* ldev = (struct lix_dev*)dev_get_drvdata(dev);
	printk("Call %s line %d\n",__FUNCTION__,__LINE__);

	return set_val(ldev, buf, count);
}

/*定义设备属性*/
static DEVICE_ATTR(my_attr, S_IRUGO | S_IWUSR, my_attr_show, my_attr_store);

#if(LINUX_VERSION_CODE>=KERNEL_VERSION(2,6,37))
/*proc文件操作方法*/
static int my_proc_open(struct inode* inode, struct file* filp);
static int my_proc_release(struct inode* inode, struct file* filp);
static ssize_t my_proc_read(struct file* filp, char __user *buf, size_t count, loff_t* f_pos);
static ssize_t my_proc_write(struct file* filp, const char __user *buf, size_t count, loff_t* f_pos);

/*proc文件操作方法表*/
static struct file_operations my_proc_fops = {
	.owner = THIS_MODULE,
	.open = my_proc_open,
	.release = my_proc_release,
	.read = my_proc_read,
	.write = my_proc_write,
};

/*打开设备方法*/
static int my_proc_open(struct inode* inode, struct file* filp)
{
	printk("Call %s line %d\n",__FUNCTION__,__LINE__);

	/*获取dev_set_drvdata保存的自定义数据设备结构体*/
	//proc的创建没有与device有相关操作，无法通过deivce私有数据获取
	//dev = container_of(inode->i_cdev, struct lix_dev, dev);

	/*将自定义数据设备结构体保存在file指针的私有数据域中，供其他获取到file指针的地方获取*/
	//使用保存的全局变量代替通过deivce私有数据获取的
	//filp->private_data = dev;
	filp->private_data = my_dev;

	return 0;
}

/*设备文件释放时调用，空实现*/
static int my_proc_release(struct inode* inode, struct file* filp)
{
	printk("Call %s line %d\n",__FUNCTION__,__LINE__);
	return 0;
}

/*读取设备的寄存器val的值*/
static ssize_t my_proc_read(struct file* filp, char __user *buf, size_t count, loff_t* f_pos)
{
	ssize_t err = 0;
	struct lix_dev* dev = filp->private_data;
	printk("Call %s line %d\n",__FUNCTION__,__LINE__);

	/*同步访问*/
	if(down_interruptible(&(dev->sem))) {
		return -ERESTARTSYS;
	}

	if(count < sizeof(dev->val)) {
		goto out;
	}

	/*将寄存器val的值拷贝到用户提供的缓冲区*/
	if(copy_to_user(buf, &(dev->val), sizeof(dev->val))) {
		err = -EFAULT;
		goto out;
	}

	err = sizeof(dev->val);

out:
	up(&(dev->sem));
	return err;
}

/*写设备的寄存器值val*/
static ssize_t my_proc_write(struct file* filp, const char __user *buf, size_t count, loff_t* f_pos)
{
	ssize_t err = 0;
	struct lix_dev* dev = filp->private_data;
	printk("Call %s line %d\n",__FUNCTION__,__LINE__);

	/*同步访问*/
	if(down_interruptible(&(dev->sem))) {
		return -ERESTARTSYS;
	}

	if(count != sizeof(dev->val)) {
		goto out;
	}

	/*将用户提供的缓冲区的值写到设备寄存器去*/
	if(copy_from_user(&(dev->val), buf, count)) {
		err = -EFAULT;
		goto out;
	}

	err = sizeof(dev->val);

out:
	up(&(dev->sem));
	return err;
}
#else
/*读取设备寄存器val的值，保存在page缓冲区中*/
static ssize_t my_proc_read(char* page, char** start, off_t off, int count, int* eof, void* data)
{
	printk("Call %s line %d\n",__FUNCTION__,__LINE__);
    if(off > 0) {
        *eof = 1;
        return 0;
    }

    return get_val(my_dev, page);
}

/*把缓冲区的值buff保存到设备寄存器val中去*/
static ssize_t my_proc_write(struct file* filp, const char __user *buff, unsigned long len, void* data)
{
    int err = 0;
    char* page = NULL;
	printk("Call %s line %d\n",__FUNCTION__,__LINE__);

    if(len > PAGE_SIZE) {
        printk(KERN_ALERT"The buff is too large: %lu.\n", len);
        return -EFAULT;
    }

    page = (char*)__get_free_page(GFP_KERNEL);
    if(!page) {
        printk(KERN_ALERT"Failed to alloc page.\n");
        return -ENOMEM;
    }

    /*先把用户提供的缓冲区值拷贝到内核缓冲区中去*/
    if(copy_from_user(page, buff, len)) {
        printk(KERN_ALERT"Failed to copy buff from user.\n");
        err = -EFAULT;
        goto out;
    }

    err = set_val(my_dev, page, len);

out:
    free_page((unsigned long)page);
    return err;
}
#endif

/*创建/proc/MY_DEVICE_PROC_NAME文件*/
struct proc_dir_entry* entry;
static void create_proc(void)
{
#if(LINUX_VERSION_CODE>=KERNEL_VERSION(2,6,37))
	entry = proc_create(MY_DEVICE_PROC_NAME, 0444, NULL, &my_proc_fops);
	if(!entry) {
		printk(KERN_ALERT"Failed to create %s proc.\n", MY_DEVICE_PROC_NAME);
	}
#else
	entry = create_proc_entry(MY_DEVICE_PROC_NAME, 0, NULL);
    if(entry) {
        entry->owner = THIS_MODULE;
        entry->read_proc = my_proc_read;
        entry->write_proc = my_proc_write;
    }else {
		printk(KERN_ALERT"Failed to create %s proc.\n", MY_DEVICE_PROC_NAME);
	}
#endif
}

/*删除/proc/MY_DEVICE_PROC_NAME文件*/
static void remove_proc(void)
{
	if(entry) {
#if(LINUX_VERSION_CODE>=KERNEL_VERSION(2,6,37))
		proc_remove(entry);
#else
		remove_proc_entry(MY_DEVICE_PROC_NAME, NULL);
#endif
	}
}

/*初始化设备*/
static int setup_dev(struct lix_dev* dev)
{
	int err;
	dev_t devno = MKDEV(my_dev_major, my_dev_minor);

	memset(dev, 0, sizeof(struct lix_dev));

	cdev_init(&(dev->dev), &my_dev_fops);
	dev->dev.owner = THIS_MODULE;
	dev->dev.ops = &my_dev_fops;

	/*注册字符设备*/
	err = cdev_add(&(dev->dev),devno, 1);
	if(err) {
		return err;
	}

	/*初始化信号量和寄存器val的值*/
#if(LINUX_VERSION_CODE>=KERNEL_VERSION(2,6,37))
	sema_init(&(dev->sem), 1);
#else
	init_MUTEX(&(dev->sem));
#endif
	dev->val = 0;

	return 0;
}

/*模块加载方法*/
static int __init my_driver_init(void)
{
	int err = -1;
	dev_t dev = 0;
	struct device* temp = NULL;

	printk(KERN_ALERT"Initializing lix device.\n");

	/*动态分配主设备和从设备号*/
	err = alloc_chrdev_region(&dev, 0, 1, MY_DEVICE_NODE_NAME);
	if(err < 0) {
		printk(KERN_ALERT"Failed to alloc char dev %s region.\n", MY_DEVICE_NODE_NAME);
		goto fail;
	}

	my_dev_major = MAJOR(dev);
	my_dev_minor = MINOR(dev);

	/*分配lix_dev设备结构体变量*/
	my_dev = kmalloc(sizeof(struct lix_dev), GFP_KERNEL);
	if(!my_dev) {
		err = -ENOMEM;
		printk(KERN_ALERT"Failed to alloc lix_dev.\n");
		goto unregister;
	}

	/*初始化设备*/
	err = setup_dev(my_dev);
	if(err) {
		printk(KERN_ALERT"Failed to setup dev: %d.\n", err);
		goto cleanup;
	}

	/*在/sys/class/目录下创建设备类别目录MY_DEVICE_CLASS_NAME*/
	my_dev_class = class_create(THIS_MODULE, MY_DEVICE_CLASS_NAME);
	if(IS_ERR(my_dev_class)) {
		err = PTR_ERR(my_dev_class);
		printk(KERN_ALERT"Failed to create %s class.\n",MY_DEVICE_CLASS_NAME);
		goto destroy_cdev;
	}

	/*在/dev/目录和/sys/class/MY_DEVICE_CLASS_NAME目录下分别创建设备文件HELLO_DEVICE_FILE_NAME*/
	temp = device_create(my_dev_class, NULL, dev, "%s", MY_DEVICE_FILE_NAME);
	if(IS_ERR(temp)) {
		err = PTR_ERR(temp);
		printk(KERN_ALERT"Failed to create %s device.\n",MY_DEVICE_FILE_NAME);
		goto destroy_class;
	}

	/*在/sys/class/MY_DEVICE_CLASS_NAME/HELLO_DEVICE_FILE_NAME目录下创建属性文件my_attr*/
	err = device_create_file(temp, &dev_attr_my_attr);
	if(err < 0) {
		printk(KERN_ALERT"Failed to create attribute my_attr.\n");
		goto destroy_device;
	}

	/*保存自定义数据设备结构体到device->device_private->driver_data，供其他获取到device指针的地方获取*/
	dev_set_drvdata(temp, my_dev);

	/*创建/proc/MY_DEVICE_PROC_NAME文件*/
	create_proc();

	printk(KERN_ALERT"Succedded to initialize hello device.\n");
	return 0;

destroy_device:
	device_destroy(my_dev_class, dev);

destroy_class:
	class_destroy(my_dev_class);

destroy_cdev:
	cdev_del(&(my_dev->dev));

cleanup:
	kfree(my_dev);

unregister:
	unregister_chrdev_region(MKDEV(my_dev_major,my_dev_minor), 1);

fail:
	return err;
}

/*模块卸载方法*/
static void __exit my_driver_exit(void)
{
	dev_t devno = MKDEV(my_dev_major, my_dev_minor);

	printk(KERN_ALERT"Destroy lix device.\n");

	/*删除/proc/MY_DEVICE_PROC_NAME文件*/
	remove_proc();

	/*销毁设备类别和设备*/
	if(my_dev_class) {
		device_destroy(my_dev_class, MKDEV(my_dev_major, my_dev_minor));
		class_destroy(my_dev_class);
	}

	/*删除字符设备和释放设备内存*/
	if(my_dev) {
		cdev_del(&(my_dev->dev));
		kfree(my_dev);
	}

	/*释放设备号*/
	unregister_chrdev_region(devno, 1);
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Standard Linux Driver");

module_init(my_driver_init);
module_exit(my_driver_exit);
