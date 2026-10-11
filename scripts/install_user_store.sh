#!/bin/sh
ROOTFS="/data/data/com.termux/files/home/.andronix/distros/ubuntu/rootfs"

mount -t proc proc "$ROOTFS/proc" 2>/dev/null
mount -t sysfs sysfs "$ROOTFS/sys" 2>/dev/null
mount --bind /dev "$ROOTFS/dev" 2>/dev/null
mount --bind /dev/pts "$ROOTFS/dev/pts" 2>/dev/null
mount -t tmpfs tmpfs "$ROOTFS/dev/shm" 2>/dev/null

export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
export DEBIAN_FRONTEND=noninteractive

chroot "$ROOTFS" /bin/bash -c "
  export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
  # 1. Install Synaptic (Classic GUI App/Package Store)
  apt-get update -qq
  apt-get install -y -qq synaptic

  # 2. Configure Pi-Apps for user ahmet
  chown -R 10453:10453 /opt/pi-apps
  su - ahmet -c 'export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin; export DISPLAY=:0; /opt/pi-apps/install'

  # Create desktop shortcut for Pi-Apps and Synaptic
  cp /usr/share/applications/synaptic.desktop /home/ahmet/Desktop/ 2>/dev/null
  cp /usr/share/applications/pi-apps.desktop /home/ahmet/Desktop/ 2>/dev/null
  chmod +x /home/ahmet/Desktop/*.desktop 2>/dev/null
  chown -R 10453:10453 /home/ahmet/Desktop
"

umount -l "$ROOTFS/dev/shm" 2>/dev/null
umount -l "$ROOTFS/dev/pts" 2>/dev/null
umount -l "$ROOTFS/dev" 2>/dev/null
umount -l "$ROOTFS/sys" 2>/dev/null
umount -l "$ROOTFS/proc" 2>/dev/null
