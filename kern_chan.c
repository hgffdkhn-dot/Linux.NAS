#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/utsname.h>      // for init_utsname()
#include <linux/mm.h>           // for si_meminfo()
#include <linux/ktime.h>        // for ktime_get_real_ts64()
#include <linux/timekeeping.h>

#define DEVICE_NAME "kern_chan"
#define CLASS_NAME  "kern"
#define BUFFER_SIZE 4096        // 最大响应长度

static int major_num;
static struct class *chan_class = NULL;
static struct device *chan_device = NULL;
static DEFINE_MUTEX(chan_mutex);
static char *kernel_buffer = NULL;      // 存储响应内容

// ----- 命令处理函数原型 -----
static void cmd_info(void);
static void cmd_mem(void);
static void cmd_uptime(void);
static void cmd_help(void);

// ----- 命令分发表 -----
struct cmd_entry {
    const char *name;
    void (*handler)(void);
};

static const struct cmd_entry cmd_table[] = {
    {"info",    cmd_info},
    {"mem",     cmd_mem},
    {"uptime",  cmd_uptime},
    {"help",    cmd_help},
    {NULL,      NULL}
};

// ----- 各个命令的具体实现 -----

/* 显示内核版本信息 */
static void cmd_info(void)
{
    struct new_utsname *uts = init_utsname();
    snprintf(kernel_buffer, BUFFER_SIZE,
             "Kernel: %s %s %s\nArch: %s\n",
             uts->sysname, uts->release, uts->version,
             uts->machine);
}

/* 显示系统内存信息 */
static void cmd_mem(void)
{
    struct sysinfo si;
    si_meminfo(&si);
    unsigned long total = si.totalram * si.mem_unit;
    unsigned long free = si.freeram * si.mem_unit;
    unsigned long available = si.totalram - si.freeram - si.bufferram - si.sharedram;
    available *= si.mem_unit;

    snprintf(kernel_buffer, BUFFER_SIZE,
             "Total RAM:   %lu MB\n"
             "Free RAM:    %lu MB\n"
             "Available:   %lu MB\n"
             "Swap total:  %lu MB\n"
             "Swap free:   %lu MB\n",
             total / (1024 * 1024),
             free / (1024 * 1024),
             available / (1024 * 1024),
             si.totalswap * si.mem_unit / (1024 * 1024),
             si.freeswap * si.mem_unit / (1024 * 1024));
}

/* 显示系统运行时间 */
static void cmd_uptime(void)
{
    struct timespec64 ts;
    ktime_get_real_ts64(&ts);   // 获取墙上时间
    // 为了获得启动时间，我们使用内核中的 jiffies 或 ktime_get_boottime
    // 更简单：使用 do_gettimeofday 获取当前时间，但我们需要 uptime 是从启动开始
    // 这里用 ktime_get_boottime() 获取系统启动后的时间
    ktime_t boot = ktime_get_boottime();
    s64 seconds = ktime_to_ns(boot) >> 30; // 近似 (2^30 ns = 1.073s)
    // 更精确：除以 NSEC_PER_SEC
    s64 sec = ktime_to_ns(boot) / NSEC_PER_SEC;
    s64 min = sec / 60;
    s64 hour = min / 60;
    sec %= 60;
    min %= 60;

    snprintf(kernel_buffer, BUFFER_SIZE,
             "Uptime: %lldh %lldm %llds\n",
             hour, min, sec);
}

/* 帮助信息 */
static void cmd_help(void)
{
    snprintf(kernel_buffer, BUFFER_SIZE,
             "Supported commands:\n"
             "  info    - Show kernel version and architecture\n"
             "  mem     - Display memory statistics\n"
             "  uptime  - Show system uptime\n"
             "  help    - Show this help\n"
             "\nWrite a command to /dev/kern_chan, then read the response.\n");
}

// ----- 命令解析与分发函数 -----
static void process_command(const char *cmd)
{
    // 清空缓冲区（第一个字符设为 '\0'）
    kernel_buffer[0] = '\0';

    // 跳过前导空白
    while (*cmd == ' ' || *cmd == '\t')
        cmd++;

    // 查找命令表
    const struct cmd_entry *entry = cmd_table;
    while (entry->name) {
        if (strcmp(cmd, entry->name) == 0) {
            entry->handler();
            return;
        }
        entry++;
    }

    // 未匹配命令
    snprintf(kernel_buffer, BUFFER_SIZE,
             "Unknown command: '%s'\nType 'help' for supported commands.\n",
             cmd);
}

// ----- 设备操作函数 -----

static int dev_open(struct inode *inodep, struct file *filep)
{
    return 0;
}

static ssize_t dev_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset)
{
    char *cmd = NULL;
    ssize_t ret = len;

    mutex_lock(&chan_mutex);

    // 分配临时空间用于存储用户命令（最大 BUFFER_SIZE - 1）
    if (len > BUFFER_SIZE - 1) {
        ret = -EINVAL;
        goto out_unlock;
    }

    cmd = kmalloc(len + 1, GFP_KERNEL);
    if (!cmd) {
        ret = -ENOMEM;
        goto out_unlock;
    }

    // 从用户空间复制命令
    if (copy_from_user(cmd, buffer, len)) {
        ret = -EFAULT;
        goto out_free;
    }
    cmd[len] = '\0';

    // 移除末尾换行符（\n 或 \r\n）
    if (len > 0 && cmd[len - 1] == '\n')
        cmd[len - 1] = '\0';
    if (len > 1 && cmd[len - 2] == '\r')
        cmd[len - 2] = '\0';   // 处理 Windows 换行

    // 处理命令（结果存入 kernel_buffer）
    process_command(cmd);

out_free:
    kfree(cmd);
out_unlock:
    mutex_unlock(&chan_mutex);
    return ret;
}

static ssize_t dev_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset)
{
    ssize_t ret;

    mutex_lock(&chan_mutex);
    if (!kernel_buffer || kernel_buffer[0] == '\0') {
        ret = 0;   // 无数据
    } else {
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

// ----- 模块初始化和退出 -----

static int __init init_chan(void)
{
    int ret;

    // 分配内核缓冲区
    kernel_buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!kernel_buffer)
        return -ENOMEM;
    kernel_buffer[0] = '\0';

    major_num = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_num < 0) {
        ret = major_num;
        goto free_buf;
    }

    chan_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(chan_class)) {
        ret = PTR_ERR(chan_class);
        goto unregister;
    }

    chan_device = device_create(chan_class, NULL, MKDEV(major_num, 0), NULL, DEVICE_NAME);
    if (IS_ERR(chan_device)) {
        ret = PTR_ERR(chan_device);
        goto destroy_class;
    }

    pr_info("KernChan loaded. Device: /dev/%s\n", DEVICE_NAME);
    return 0;

destroy_class:
    class_destroy(chan_class);
unregister:
    unregister_chrdev(major_num, DEVICE_NAME);
free_buf:
    kfree(kernel_buffer);
    return ret;
}

static void __exit exit_chan(void)
{
    device_destroy(chan_class, MKDEV(major_num, 0));
    class_destroy(chan_class);
    unregister_chrdev(major_num, DEVICE_NAME);
    kfree(kernel_buffer);
    pr_info("KernChan unloaded.\n");
}

module_init(init_chan);
module_exit(exit_chan);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Team");
MODULE_DESCRIPTION("Temporary kernel channel with command dispatch");
