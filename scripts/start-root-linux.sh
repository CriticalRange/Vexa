#!/data/data/com.termux/files/usr/bin/bash
# ==============================================================================
# VEXA / Termux Native Root Chroot Launcher (Ubuntu 26.04 + XFCE + Box64 + X11)
# ==============================================================================

# 1. PulseAudio (Gecikmesiz ses sunucusu)
if ! pgrep -x pulseaudio > /dev/null 2>&1; then
    echo "[+] PulseAudio ses sunucusu başlatılıyor..."
    pulseaudio --start --load="module-native-protocol-tcp auth-ip-acl=127.0.0.1 auth-anonymous=1" --exit-idle-time=-1 2>/dev/null
fi

# 2. Termux-X11 Sunucusu
if ! pgrep -f "termux-x11" > /dev/null 2>&1; then
    echo "[+] Termux-X11 grafik sunucusu başlatılıyor..."
    termux-x11 :0 -ac &
    sleep 2
fi

# 3. Termux-X11 Uygulamasını Ekrana Getir
am start --user 0 -n com.termux.x11/com.termux.x11.MainActivity >/dev/null 2>&1

# 4. Root Chroot Ortamını Başlat
echo "[+] Root Chroot (Ubuntu + XFCE) başlatılıyor..."
su -c '
setenforce 0 2>/dev/null
chmod 666 /dev/dri/* 2>/dev/null

ROOTFS="/data/data/com.termux/files/home/.andronix/distros/ubuntu/rootfs"

# Mounts
mount -t proc proc "$ROOTFS/proc" 2>/dev/null
mount -t sysfs sysfs "$ROOTFS/sys" 2>/dev/null
mount --bind /dev "$ROOTFS/dev" 2>/dev/null
mount --bind /dev/pts "$ROOTFS/dev/pts" 2>/dev/null
mount -t tmpfs -o size=1G tmpfs "$ROOTFS/dev/shm" 2>/dev/null
mount --bind /data/data/com.termux/files/usr/tmp "$ROOTFS/tmp" 2>/dev/null
mkdir -p "$ROOTFS/sdcard" 2>/dev/null
mount --bind /sdcard "$ROOTFS/sdcard" 2>/dev/null

# DNS
echo "nameserver 8.8.8.8" > "$ROOTFS/etc/resolv.conf"

# Launch XFCE Desktop
chroot "$ROOTFS" /usr/bin/env -i \
  HOME=/root \
  USER=root \
  TERM=xterm-256color \
  DISPLAY=:0 \
  PULSE_SERVER=127.0.0.1:4713 \
  PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
  /bin/bash -c "dbus-launch --exit-with-session startxfce4"
'
