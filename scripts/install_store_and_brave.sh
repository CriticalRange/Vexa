#!/bin/sh
ROOTFS="/data/data/com.termux/files/home/.andronix/distros/ubuntu/rootfs"

# Mounts for apt and execution
mount -t proc proc "$ROOTFS/proc" 2>/dev/null
mount -t sysfs sysfs "$ROOTFS/sys" 2>/dev/null
mount --bind /dev "$ROOTFS/dev" 2>/dev/null
mount --bind /dev/pts "$ROOTFS/dev/pts" 2>/dev/null
mount -t tmpfs tmpfs "$ROOTFS/dev/shm" 2>/dev/null
echo "nameserver 8.8.8.8" > "$ROOTFS/etc/resolv.conf"

export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
export DEBIAN_FRONTEND=noninteractive

chroot "$ROOTFS" /bin/bash -c '
  export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
  export DEBIAN_FRONTEND=noninteractive

  echo "=== 1. Brave Browser Kuruluyor ==="
  curl -fsSLo /usr/share/keyrings/brave-browser-archive-keyring.gpg https://brave-browser-apt-release.s3.brave.com/brave-browser-archive-keyring.gpg
  curl -fsSLo /etc/apt/sources.list.d/brave-browser-release.sources https://brave-browser-apt-release.s3.brave.com/brave-browser.sources
  apt-get update -qq
  apt-get install -y -qq brave-browser

  # Desktop shortcut for Brave
  mkdir -p /home/ahmet/Desktop /root/Desktop
  if [ -f /usr/share/applications/brave-browser.desktop ]; then
    cp /usr/share/applications/brave-browser.desktop /home/ahmet/Desktop/
    cp /usr/share/applications/brave-browser.desktop /root/Desktop/
    chmod +x /home/ahmet/Desktop/brave-browser.desktop /root/Desktop/brave-browser.desktop 2>/dev/null
  fi

  echo "=== 2. Pi-Apps (ARM64 App Store) Kuruluyor ==="
  apt-get install -y -qq git yad xterm bc
  git clone --depth 1 https://github.com/Botspot/pi-apps /opt/pi-apps 2>/dev/null || (cd /opt/pi-apps && git pull)
  /opt/pi-apps/install

  # Desktop shortcut for Pi-Apps
  if [ -f /usr/share/applications/pi-apps.desktop ]; then
    cp /usr/share/applications/pi-apps.desktop /home/ahmet/Desktop/
    cp /usr/share/applications/pi-apps.desktop /root/Desktop/
    chmod +x /home/ahmet/Desktop/pi-apps.desktop /root/Desktop/pi-apps.desktop 2>/dev/null
  fi

  chown -R 10453:10453 /home/ahmet/Desktop 2>/dev/null
  echo "=== KURULUM TAMAMLANDI ==="
'

umount -l "$ROOTFS/dev/shm" 2>/dev/null
umount -l "$ROOTFS/dev/pts" 2>/dev/null
umount -l "$ROOTFS/dev" 2>/dev/null
umount -l "$ROOTFS/sys" 2>/dev/null
umount -l "$ROOTFS/proc" 2>/dev/null
