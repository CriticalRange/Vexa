#!/bin/sh
ROOTFS="/data/data/com.termux/files/home/.andronix/distros/ubuntu/rootfs"

mount -t proc proc "$ROOTFS/proc" 2>/dev/null
mount -t sysfs sysfs "$ROOTFS/sys" 2>/dev/null
mount --bind /dev "$ROOTFS/dev" 2>/dev/null
echo "nameserver 8.8.8.8" > "$ROOTFS/etc/resolv.conf"

export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

chroot "$ROOTFS" /bin/bash -c '
  cd /tmp
  rm -rf /opt/prismlauncher prism.tar.gz
  echo "Prism Launcher ARM64 indiriliyor..."
  curl -L -o prism.tar.gz https://github.com/PrismLauncher/PrismLauncher/releases/download/11.1.1/PrismLauncher-Linux-aarch64-Qt6-Portable-11.1.1.tar.gz
  
  mkdir -p /opt/prismlauncher
  tar -xzf prism.tar.gz -C /opt/prismlauncher --strip-components=1
  rm -f prism.tar.gz
  
  ln -sf /opt/prismlauncher/prismlauncher /usr/local/bin/prismlauncher
  chmod +x /opt/prismlauncher/prismlauncher /usr/local/bin/prismlauncher
  
  mkdir -p /home/ahmet/Desktop /root/Desktop /usr/share/applications
  cat << DESK > /usr/share/applications/prismlauncher.desktop
[Desktop Entry]
Version=1.0
Name=Prism Launcher
Comment=Minecraft Launcher
Exec=/usr/local/bin/prismlauncher
Icon=/opt/prismlauncher/prismlauncher.png
Terminal=false
Type=Application
Categories=Game;
DESK

  cp /usr/share/applications/prismlauncher.desktop /home/ahmet/Desktop/
  cp /usr/share/applications/prismlauncher.desktop /root/Desktop/
  chmod +x /home/ahmet/Desktop/prismlauncher.desktop /root/Desktop/prismlauncher.desktop
  chown -R 10453:10453 /home/ahmet/Desktop
  
  echo "Kurulum tamamlandi:"
  ls -lh /usr/local/bin/prismlauncher
'

umount -l "$ROOTFS/dev" 2>/dev/null
umount -l "$ROOTFS/sys" 2>/dev/null
umount -l "$ROOTFS/proc" 2>/dev/null
