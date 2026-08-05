# Copyright (C) 2026 Ardaninho
# SPDX-License-Identifier: GPL-3.0-or-later

# Mount Virtual Disk
losetup /dev/block/loop0 /data/media/0/archdroid/linux.img # ANDROID SHELL
mkdir /mnt/archdroid # ANDROID SHELL
mount -t ext4 -o loop /dev/block/loop0 /mnt/archdroid/ # ANDROID SHELL

# Mount pseudo filesystems
mount -t tmpfs tmpfs /mnt/archdroid/run # ANDROID SHELL
mount -t proc proc /mnt/archdroid/proc # ANDROID SHELL
mount -t sysfs sys /mnt/archdroid/sys # ANDROID SHELL
mount -o bind /dev /mnt/archdroid/dev # ANDROID SHELL
mount -t devpts devpts /mnt/archdroid/dev/pts # ANDROID SHELL
mount -t tmpfs tmpfs /mnt/archdroid/tmp # ANDROID SHELL

# Chroot command
chroot /mnt/archdroid /usr/bin/env -i HOME=/root PATH="/usr/bin:/usr/sbin:/usr/local/bin:/bin:/sbin" DISPLAY=:0 XDG_RUNTIME_DIR=/tmp /bin/bash # ANDROID SHELL

# Stop Android UI 
sync; pkill -STOP system_server; pkill -STOP surfaceflinger # ANDROID SHELL

# Start DE (all cmds here are chroot only)
fb_refresh /dev/graphics/fb0 60 & startx # With logs # CHROOT SHELL
fb_refresh /dev/graphics/fb0 60 > /dev/null 2>&1 & startx # Without logs # CHROOT SHELL

# Resume Android UI
killall fb_refresh
pkill -CONT surfaceflinger; pkill -CONT system_server # ANDROID SHELL

# Networking (first install only)
rm /mnt/archdroid/etc/resolv.conf # ANDROID SHELL
echo "nameserver $(getprop net.dns1)" > /mnt/archdroid/etc/resolv.conf # ANDROID SHELL
