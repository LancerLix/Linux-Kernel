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

/*���豸�ʹ��豸�ű���*/
static int my_dev_major = 0;
static int my_dev_minor = 0;

/*�豸�����豸����*/
static struct class* my_dev_class = NULL;
static struct lix_dev* my_dev = NULL;

/*��ͳ���豸�ļ���������*/
static int my_dev_open(struct inode* inode, struct file* filp);
static int my_dev_release(struct inode* inode, struct file* filp);
static ssize_t my_dev_read(struct file* filp, char __user *buf, size_t count, loff_t* f_pos);
static ssize_t my_dev_write(struct file* filp, const char __user *buf, size_t count, loff_t* f_pos);

/*�豸�ļ�����������*/
static struct file_operations my_dev_fops = {
	.owner = THIS_MODULE,
	.open = my_dev_open,
	.release = my_dev_release,
	.read = my_dev_read,
	.write = my_dev_write,
};

/*���豸����*/
static int my_dev_open(struct inode* inode, struct file* filp)
{
	struct lix_dev* dev;
	printk("Call %s line %d\n",__FUNCTION__,__LINE__);
	
	/*��ȡdev_set_drvdata������Զ��������豸�ṹ��*/
	dev = container_of(inode->i_cdev, struct lix_dev, dev);
	/*���Զ��������豸�ṹ�屣����fileָ���˽���������У���������ȡ��fileָ��ĵط���ȡ*/
	filp->private_data = dev;

	return 0;
}

/*�豸�ļ��ͷ�ʱ���ã���ʵ��*/
static int my_dev_release(struct inode* inode, struct file* filp)
{
	printk("Call %s line %d\n",__FUNCTION__,__LINE__);
	return 0;
}

/*��ȡ�豸�ļĴ���val��ֵ*/
static ssize_t my_dev_read(struct file* filp, char __user *buf, size_t count, loff_t* f_pos)
{
	ssize_t err = 0;
	struct lix_dev* dev = filp->private_data;
	printk("Call %s line %d\n",__FUNCTION__,__LINE__);

	/*ͬ������*/
	if(down_interruptible(&(dev->sem))) {
		return -ERESTARTSYS;
	}

	if(count < sizeof(dev->val)) {
		goto out;
	}

	/*���Ĵ���val��ֵ�������û��ṩ�Ļ�����*/
	if(copy_to_user(buf, &(dev->val), sizeof(dev->val))) {
		err = -EFAULT;
		goto out;
	}

	err = sizeof(dev->val);

out:
	up(&(dev->sem));
	return err;
}

/*д�豸�ļĴ���ֵval*/
static ssize_t my_dev_write(struct file* filp, const char __user *buf, size_t count, loff_t* f_pos)
{
	ssize_t err = 0;
	struct lix_dev* dev = filp->private_data;
	printk("Call %s line %d\n",__FUNCTION__,__LINE__);

	/*ͬ������*/
	if(down_interruptible(&(dev->sem))) {
		return -ERESTARTSYS;
	}

	if(count != sizeof(dev->val)) {
		goto out;
	}

	/*���û��ṩ�Ļ�������ֵд���豸�Ĵ���ȥ*/
	if(copy_from_user(&(dev->val), buf, count)) {
		err = -EFAULT;
		goto out;
	}

	err = sizeof(dev->val);

out:
	up(&(dev->sem));
	return err;
}

/*�����������Է���*/
static ssize_t my_attr_show(struct device* dev, struct device_attribute* attr,  char* buf);
static ssize_t my_attr_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);

/*��ȡ�Ĵ���val��ֵ��������buf�У��ڲ�ʹ��*/
static ssize_t get_val(struct lix_dev* dev, char* buf)
{
	int val = 0;

	/*ͬ������*/
	if(down_interruptible(&(dev->sem))) {
		return -ERESTARTSYS;
	}

	val = dev->val;
	up(&(dev->sem));

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

/*�ѻ�����buf��ֵд���豸�Ĵ���val��ȥ���ڲ�ʹ��*/
static ssize_t set_val(struct lix_dev* dev, const char* buf, size_t count)
{
	int val = 0;

	/*���ַ���ת��������*/
	val = simple_strtol(buf, NULL, 10);

	/*ͬ������*/
	if(down_interruptible(&(dev->sem))) {
		return -ERESTARTSYS;
	}

	dev->val = val;
	up(&(dev->sem));

	return count;
}

/*��ȡ�豸����val*/
static ssize_t my_attr_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	struct lix_dev* ldev = (struct lix_dev*)dev_get_drvdata(dev);
	printk("Call %s line %d\n",__FUNCTION__,__LINE__);

	return get_val(ldev, buf);
}

/*д�豸����val*/
static ssize_t my_attr_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
	struct lix_dev* ldev = (struct lix_dev*)dev_get_drvdata(dev);
	printk("Call %s line %d\n",__FUNCTION__,__LINE__);

	return set_val(ldev, buf, count);
}

/*�����豸����*/
static DEVICE_ATTR(my_attr, S_IRUGO | S_IWUSR, my_attr_show, my_attr_store);

#if(LINUX_VERSION_CODE>=KERNEL_VERSION(2,6,37))
/*proc�ļ���������*/
static int my_proc_open(struct inode* inode, struct file* filp);
static int my_proc_release(struct inode* inode, struct file* filp);
static ssize_t my_proc_read(struct file* filp, char __user *buf, size_t count, loff_t* f_pos);
static ssize_t my_proc_write(struct file* filp, const char __user *buf, size_t count, loff_t* f_pos);

/*proc�ļ�����������*/
static struct file_operations my_proc_fops = {
	.owner = THIS_MODULE,
	.open = my_proc_open,
	.release = my_proc_release,
	.read = my_proc_read,
	.write = my_proc_write,
};

/*���豸����*/
static int my_proc_open(struct inode* inode, struct file* filp)
{
	printk("Call %s line %d\n",__FUNCTION__,__LINE__);

	/*��ȡdev_set_drvdata������Զ��������豸�ṹ��*/
	//proc�Ĵ���û����device����ز������޷�ͨ��deivce˽�����ݻ�ȡ
	//dev = container_of(inode->i_cdev, struct lix_dev, dev);

	/*���Զ��������豸�ṹ�屣����fileָ���˽���������У���������ȡ��fileָ��ĵط���ȡ*/
	//ʹ�ñ����ȫ�ֱ�������ͨ��deivce˽�����ݻ�ȡ��
	//filp->private_data = dev;
	filp->private_data = my_dev;

	return 0;
}

/*�豸�ļ��ͷ�ʱ���ã���ʵ��*/
static int my_proc_release(struct inode* inode, struct file* filp)
{
	printk("Call %s line %d\n",__FUNCTION__,__LINE__);
	return 0;
}

/*��ȡ�豸�ļĴ���val��ֵ*/
static ssize_t my_proc_read(struct file* filp, char __user *buf, size_t count, loff_t* f_pos)
{
	ssize_t err = 0;
	struct lix_dev* dev = filp->private_data;
	printk("Call %s line %d\n",__FUNCTION__,__LINE__);

	/*ͬ������*/
	if(down_interruptible(&(dev->sem))) {
		return -ERESTARTSYS;
	}

	if(count < sizeof(dev->val)) {
		goto out;
	}

	/*���Ĵ���val��ֵ�������û��ṩ�Ļ�����*/
	if(copy_to_user(buf, &(dev->val), sizeof(dev->val))) {
		err = -EFAULT;
		goto out;
	}

	err = sizeof(dev->val);

out:
	up(&(dev->sem));
	return err;
}

/*д�豸�ļĴ���ֵval*/
static ssize_t my_proc_write(struct file* filp, const char __user *buf, size_t count, loff_t* f_pos)
{
	ssize_t err = 0;
	struct lix_dev* dev = filp->private_data;
	printk("Call %s line %d\n",__FUNCTION__,__LINE__);

	/*ͬ������*/
	if(down_interruptible(&(dev->sem))) {
		return -ERESTARTSYS;
	}

	if(count != sizeof(dev->val)) {
		goto out;
	}

	/*���û��ṩ�Ļ�������ֵд���豸�Ĵ���ȥ*/
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
/*��ȡ�豸�Ĵ���val��ֵ��������page��������*/
static ssize_t my_proc_read(char* page, char** start, off_t off, int count, int* eof, void* data)
{
	printk("Call %s line %d\n",__FUNCTION__,__LINE__);
    if(off > 0) {
        *eof = 1;
        return 0;
    }

    return get_val(my_dev, page);
}

/*�ѻ�������ֵbuff���浽�豸�Ĵ���val��ȥ*/
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

    /*�Ȱ��û��ṩ�Ļ�����ֵ�������ں˻�������ȥ*/
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

/*����/proc/MY_DEVICE_PROC_NAME�ļ�*/
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

/*ɾ��/proc/MY_DEVICE_PROC_NAME�ļ�*/
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

/*��ʼ���豸*/
static int setup_dev(struct lix_dev* dev)
{
	int err;
	dev_t devno = MKDEV(my_dev_major, my_dev_minor);

	memset(dev, 0, sizeof(struct lix_dev));

	cdev_init(&(dev->dev), &my_dev_fops);
	dev->dev.owner = THIS_MODULE;
	dev->dev.ops = &my_dev_fops;

	/*ע���ַ��豸*/
	err = cdev_add(&(dev->dev),devno, 1);
	if(err) {
		return err;
	}

	/*��ʼ���ź����ͼĴ���val��ֵ*/
#if(LINUX_VERSION_CODE>=KERNEL_VERSION(2,6,37))
	sema_init(&(dev->sem), 1);
#else
	init_MUTEX(&(dev->sem));
#endif
	dev->val = 0;

	return 0;
}

/*ģ����ط���*/
static int __init my_driver_init(void)
{
	int err = -1;
	dev_t dev = 0;
	struct device* temp = NULL;

	printk(KERN_ALERT"Initializing lix device.\n");

	/*��̬�������豸�ʹ��豸��*/
	err = alloc_chrdev_region(&dev, 0, 1, MY_DEVICE_NODE_NAME);
	if(err < 0) {
		printk(KERN_ALERT"Failed to alloc char dev %s region.\n", MY_DEVICE_NODE_NAME);
		goto fail;
	}

	my_dev_major = MAJOR(dev);
	my_dev_minor = MINOR(dev);

	/*����lix_dev�豸�ṹ�����*/
	my_dev = kmalloc(sizeof(struct lix_dev), GFP_KERNEL);
	if(!my_dev) {
		err = -ENOMEM;
		printk(KERN_ALERT"Failed to alloc lix_dev.\n");
		goto unregister;
	}

	/*��ʼ���豸*/
	err = setup_dev(my_dev);
	if(err) {
		printk(KERN_ALERT"Failed to setup dev: %d.\n", err);
		goto cleanup;
	}

	/*��/sys/class/Ŀ¼�´����豸���Ŀ¼MY_DEVICE_CLASS_NAME*/
	my_dev_class = class_create(THIS_MODULE, MY_DEVICE_CLASS_NAME);
	if(IS_ERR(my_dev_class)) {
		err = PTR_ERR(my_dev_class);
		printk(KERN_ALERT"Failed to create %s class.\n",MY_DEVICE_CLASS_NAME);
		goto destroy_cdev;
	}

	/*��/dev/Ŀ¼��/sys/class/MY_DEVICE_CLASS_NAMEĿ¼�·ֱ𴴽��豸�ļ�HELLO_DEVICE_FILE_NAME*/
	temp = device_create(my_dev_class, NULL, dev, "%s", MY_DEVICE_FILE_NAME);
	if(IS_ERR(temp)) {
		err = PTR_ERR(temp);
		printk(KERN_ALERT"Failed to create %s device.\n",MY_DEVICE_FILE_NAME);
		goto destroy_class;
	}

	/*��/sys/class/MY_DEVICE_CLASS_NAME/HELLO_DEVICE_FILE_NAMEĿ¼�´��������ļ�my_attr*/
	err = device_create_file(temp, &dev_attr_my_attr);
	if(err < 0) {
		printk(KERN_ALERT"Failed to create attribute my_attr.\n");
		goto destroy_device;
	}

	/*�����Զ��������豸�ṹ�嵽device->device_private->driver_data����������ȡ��deviceָ��ĵط���ȡ*/
	dev_set_drvdata(temp, my_dev);

	/*����/proc/MY_DEVICE_PROC_NAME�ļ�*/
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

/*ģ��ж�ط���*/
static void __exit my_driver_exit(void)
{
	dev_t devno = MKDEV(my_dev_major, my_dev_minor);

	printk(KERN_ALERT"Destroy lix device.\n");

	/*ɾ��/proc/MY_DEVICE_PROC_NAME�ļ�*/
	remove_proc();

	/*�����豸�����豸*/
	if(my_dev_class) {
		device_destroy(my_dev_class, MKDEV(my_dev_major, my_dev_minor));
		class_destroy(my_dev_class);
	}

	/*ɾ���ַ��豸���ͷ��豸�ڴ�*/
	if(my_dev) {
		cdev_del(&(my_dev->dev));
		kfree(my_dev);
	}

	/*�ͷ��豸��*/
	unregister_chrdev_region(devno, 1);
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Standard Linux Driver");

module_init(my_driver_init);
module_exit(my_driver_exit);
