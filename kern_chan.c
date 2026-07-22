#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#define DEVICE_NAME "kern_chan"
#define CLASS_NAME  "kern"

static int major_num;
static struct class *chan_class = NULL;
static struct device *chan_device = NULL;
static DEFINE_MUTEX(chan_mutex);
static char *kernel_buffer = NULL;

// 打开设备
static int dev_open(struct inode *inodep, struct file *filep) {
    return 0;
}

// 用户写入数据（接收指令）
static ssize_t dev_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset) {
    mutex_lock(&chan_mutex);
    // 实际逻辑：解析buffer中的指令，执行内核操作
    // 此处示例：将用户数据暂存
    if (kernel_buffer) kfree(kernel_buffer);
    kernel_buffer = kmalloc(len + 1, GFP_KERNEL);
    if (!kernel_buffer) { mutex_unlock(&chan_mutex); return -ENOMEM; }
    
    if (copy_from_user(kernel_buffer, buffer, len)) {
        kfree(kernel_buffer);
        mutex_unlock(&chan_mutex);
        return -EFAULT;
    }
    kernel_buffer[len] = '\0';
    mutex_unlock(&chan_mutex);
    return len;
}

// 用户读取数据（获取内核信息）
static ssize_t dev_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset) {
    int ret = 0;
    mutex_lock(&chan_mutex);
    if (kernel_buffer) {
        ret = simple_read_from_buffer(buffer, len, offset, kernel_buffer, strlen(kernel_buffer));
    }
    mutex_unlock(&chan_mutex);
    return ret;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
};

static int __init init_chan(void) {
    major_num = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_num < 0) return major_num;

    chan_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(chan_class)) { unregister_chrdev(major_num, DEVICE_NAME); return PTR_ERR(chan_class); }

    chan_device = device_create(chan_class, NULL, MKDEV(major_num, 0), NULL, DEVICE_NAME);
    if (IS_ERR(chan_device)) { class_destroy(chan_class); unregister_chrdev(major_num, DEVICE_NAME); return PTR_ERR(chan_device); }

    pr_info("Kern Chan loaded. Device: /dev/%s\n", DEVICE_NAME);
    return 0;
}

static void __exit exit_chan(void) {
    device_destroy(chan_class, MKDEV(major_num, 0));
    class_destroy(chan_class);
    unregister_chrdev(major_num, DEVICE_NAME);
    if (kernel_buffer) kfree(kernel_buffer);
    pr_info("Kern Chan unloaded.\n");
}

module_init(init_chan);
module_exit(exit_chan);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Team");