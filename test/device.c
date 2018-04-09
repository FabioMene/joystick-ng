#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/types.h>
#include <linux/device.h>

#define LOG_TAG "[test-device]"

#define printd(f, a...) printk(KERN_INFO  LOG_TAG "[Dbg ] " f "\n", ##a)
#define printw(f, a...) printk(KERN_ALERT LOG_TAG "[Warn] " f "\n", ##a)
#define printe(f, a...) printk(KERN_ERR   LOG_TAG "[Err ] " f "\n", ##a)

int     test_open(struct inode*, struct file*);
int     test_release(struct inode*, struct file*);
ssize_t test_read(struct file*, char*, size_t, loff_t *);
ssize_t test_write(struct file*, const char*, size_t, loff_t *);

#define DEVICE_NAME "test-dev"

static int device_set_permissions(struct device* dev, struct kobj_uevent_env* env){
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

dev_t          dev_base;
struct cdev*   dev_cdev;
struct class*  dev_class;
struct device* devices[32];

struct file_operations fops = {
    .read    = test_read,
    .write   = test_write,
    .open    = test_open,
    .release = test_release
};

int init_module(void){
    int i;
    struct device* dev;
    dev_class = class_create(THIS_MODULE, "test-device-class");
    if(dev_class == NULL){
        printe("Impossibile creare una classe device\n");
        return 1;
    }
    dev_class->dev_uevent = device_set_permissions;
    alloc_chrdev_region(&dev_base, 0, 32, DEVICE_NAME);
    dev_cdev = cdev_alloc();
    cdev_init(dev_cdev, &fops);
    cdev_add(dev_cdev, dev_base, 32);
    for(i=0;i < 32;i++){
        dev = device_create(dev_class, NULL, MKDEV(MAJOR(dev_base), i), NULL, "jng%d", i);
        if(dev == NULL){
            printe("Impossibile creare il device");
            return 1;
        }
        devices[i] = dev;
    }
    return 0;
}

void cleanup_module(void){
    int i;
    for(i=0;i < 32;i++) device_destroy(dev_class, MKDEV(MAJOR(dev_base), i));
    cdev_del(dev_cdev);
    unregister_chrdev_region(dev_base, 32);
    class_destroy(dev_class);
}

static inline int get_jd(struct file* fp){
    return (int)fp->private_data;
}

int test_open(struct inode* inodp, struct file* fp){
    fp->private_data = (void*)MINOR(inodp->i_rdev);
    try_module_get(THIS_MODULE);
    return 0;
}

int test_release(struct inode* inodp, struct file* fp){
    module_put(THIS_MODULE);
    return 0;
}

ssize_t test_read(struct file* fp, char* user_buffer, size_t length, loff_t * offset){
    char buffer[16];
    if(length < 16) return -EINVAL;
    length = snprintf(buffer, 16, "%d - %d", get_jd(fp), current->pid);
    copy_to_user(user_buffer, buffer, length);
    return length;
}

ssize_t test_write(struct file *fp, const char *buff, size_t len, loff_t * off){
    return -EINVAL;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fabio Meneghetti <fabiomene97@gmail.com>");
MODULE_DESCRIPTION("Modulo test");
