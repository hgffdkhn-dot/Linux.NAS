KernChan - Temporary Kernel Access Channel for Android

KernChan is a lightweight, temporary Linux kernel module that creates a secure, user‑space accessible communication channel directly into the kernel. It is designed for Redmi K40 Gaming Edition (kernel 4.14.186) but can be adapted to other Android devices.

Once loaded, the module exposes a character device (/dev/kern_chan) that allows any script or program to send commands and receive responses from the kernel – without the need for risky hacks or permanent system modifications. The module disappears after a reboot, keeping your device clean and safe.

---

✨ Features

· Temporary & Clean – The module resides only in RAM; a simple reboot removes it completely.
· No Kernel Patching – Uses a standard, dynamically loadable kernel module (.ko).
· Simple I/O Interface – Communicate via read/write or echo/cat on /dev/kern_chan.
· Pre‑built for Your Device – GitHub Actions compiles the module using your actual kernel config, ensuring maximum compatibility.
· One‑Command Install – A single install.sh script decodes and loads the module.

---

📋 Prerequisites

Requirement Details
Device Redmi K40 Gaming Edition (ares) with kernel 4.14.186
Root Access The module must be loaded with insmod, which requires root privileges.
Bootloader Unlocked (to allow loading custom modules).
Storage ~10 MB free space for the installer script.
Tools adb (to push the script) and a terminal emulator or adb shell.

Note: The module is built specifically for kernel 4.14.186. If your device runs a different kernel, you will need to extract your own .config and rebuild (see Building).

---

📦 Getting Started

1. Download the Installer

The project is built automatically via GitHub Actions.
Download the latest installer.zip from the Actions tab of this repository.

Unzip it – you will get a single file: install.sh.

2. Push the Script to Your Device

```bash
adb push install.sh /data/local/tmp/
```

3. Execute the Script (as root)

Open a terminal on your device (or use adb shell) and run:

```bash
su
sh /data/local/tmp/install.sh
```

The script will:

· Decode the embedded kernel module.
· Load it using insmod.
· Create the device node /dev/kern_chan.

If successful, you will see:

```
========================================
>>> Kernel interface address: /dev/kern_chan
>>> (Reboot to remove the module automatically)
========================================
```

---

🔌 Using the Channel

Once loaded, any user‑space program can interact with the kernel via /dev/kern_chan.

Write (send a command)

```bash
echo "get_cpu_info" > /dev/kern_chan
```

Read (get response)

```bash
cat /dev/kern_chan
```

In a C program

```c
int fd = open("/dev/kern_chan", O_RDWR);
write(fd, "some_command", strlen("some_command"));
char buf[256];
read(fd, buf, sizeof(buf));
close(fd);
```

---

🏗️ Building (for Advanced Users)

If you want to build the module yourself (e.g., for a different kernel version):

1. Extract your kernel config
   ```bash
   adb shell "zcat /proc/config.gz" > kernel_config
   ```
   (If /proc/config.gz is not available, look for /boot/config-$(uname -r).)
2. Place the config in the repository root.
3. Trigger the GitHub Actions workflow – it will automatically:
   · Fetch the kernel source for your device.
   · Apply your config.
   · Cross‑compile the module (kern_chan.ko).
   · Package it into install.sh.

You can also build locally using a suitable cross‑compiler (e.g., gcc-aarch64-linux-gnu).

---

🔒 Security Considerations

· Permission: The device node is created with mode 0660 – only root and users in the owning group can access it.
· Input validation: All data from user space is copied safely using copy_from_user and copy_to_user.
· Concurrency: A mutex protects the internal buffer from race conditions.
· Temporary nature: The module is automatically unloaded on reboot, leaving no persistent footprint.

⚠️ Warning: This module gives direct kernel access. Use it only with trusted scripts and avoid exposing the device node to untrusted processes.

---

📜 License

This project is licensed under the GPL v2 (same as the Linux kernel).
See the LICENSE file for details.

---

🤝 Contributing

Issues and pull requests are welcome!
Feel free to improve the kernel code, the installer script, or the build pipeline.

---

📬 Contact / Support

· Author: Your Team
· GitHub: hsfdkhn-dot/Linux.NAS

For questions, open an issue or reach out via the repository discussions.

---

Enjoy safe, temporary kernel access!


