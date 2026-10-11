#!/data/data/com.termux/files/usr/bin/bash
# ==============================================================================
# Ultra-Light Standalone App Launcher (Zero Desktop / Pure Performance)
# ==============================================================================

CMD="${1:-prism}"

if [ "$CMD" = "stop" ]; then
    echo "[+] Servisler kapatiliyor..."
    su -c '
    ROOTFS="/data/data/com.termux/files/home/.andronix/distros/ubuntu/rootfs"
    fuser -k -9 "$ROOTFS" 2>/dev/null
    umount -l "$ROOTFS/tmp" 2>/dev/null
    umount -l "$ROOTFS/sdcard" 2>/dev/null
    umount -l "$ROOTFS/dev/shm" 2>/dev/null
    umount -l "$ROOTFS/dev/pts" 2>/dev/null
    umount -l "$ROOTFS/dev" 2>/dev/null
    umount -l "$ROOTFS/sys" 2>/dev/null
    umount -l "$ROOTFS/proc" 2>/dev/null
    '
    pkill -f termux-x11 2>/dev/null
    pulseaudio -k 2>/dev/null
    echo "[+] Tum servisler durduruldu."
    exit 0
fi

# 1. PulseAudio (Ses)
if ! pgrep -x pulseaudio > /dev/null 2>&1; then
    pulseaudio --start --load="module-native-protocol-tcp auth-ip-acl=127.0.0.1 auth-anonymous=1" --exit-idle-time=-1 2>/dev/null
fi

# 2. Termux-X11 (Ekran)
if ! pgrep -f "termux-x11" > /dev/null 2>&1; then
    termux-x11 :0 -ac &
    sleep 1
fi

# 3. Termux-X11 Ekrana Getir
am start --user 0 -n com.termux.x11/com.termux.x11.MainActivity >/dev/null 2>&1

# 4. Uygulama Secimi
case "$CMD" in
    prism|mc|minecraft)
        APP_EXEC="openbox & prismlauncher"
        echo "[+] Prism Launcher aciliyor (Masaustu yok, saf oyun modu)..."
        ;;
    brave|browser)
        APP_EXEC="openbox & brave-browser --no-sandbox"
        echo "[+] Brave Browser aciliyor..."
        ;;
    store|synaptic)
        APP_EXEC="openbox & synaptic"
        echo "[+] App Store (Synaptic) aciliyor..."
        ;;
    term|terminal)
        APP_EXEC="openbox & xterm -fa Monospace -fs 12"
        echo "[+] Linux Terminal aciliyor..."
        ;;
    desktop|xfce)
        APP_EXEC="dbus-launch --exit-with-session startxfce4"
        echo "[+] Tam XFCE Masaustu aciliyor..."
        ;;
    *)
        APP_EXEC="openbox & $CMD"
        echo "[+] $CMD aciliyor..."
        ;;
esac

# 5. Chroot Calistir
su -c "
setenforce 0 2>/dev/null
chmod 666 /dev/dri/* 2>/dev/null

ROOTFS=\"/data/data/com.termux/files/home/.andronix/distros/ubuntu/rootfs\"

mount -t proc proc \"\$ROOTFS/proc\" 2>/dev/null
mount -t sysfs sysfs \"\$ROOTFS/sys\" 2>/dev/null
mount --bind /dev \"\$ROOTFS/dev\" 2>/dev/null
mount --bind /dev/pts \"\$ROOTFS/dev/pts\" 2>/dev/null
mount -t tmpfs -o size=1G tmpfs \"\$ROOTFS/dev/shm\" 2>/dev/null
mount --bind /data/data/com.termux/files/usr/tmp \"\$ROOTFS/tmp\" 2>/dev/null
mkdir -p \"\$ROOTFS/sdcard\" \"\$ROOTFS/data/local/tmp\" 2>/dev/null
chmod 1777 \"\$ROOTFS/data/local/tmp\" 2>/dev/null
mount --bind /sdcard \"\$ROOTFS/sdcard\" 2>/dev/null

echo 'nameserver 8.8.8.8' > \"\$ROOTFS/etc/resolv.conf\"

chroot \"\$ROOTFS\" /usr/bin/env -i \
  HOME=/root \
  USER=root \
  TERM=xterm-256color \
  DISPLAY=:0 \
  PULSE_SERVER=127.0.0.1:4713 \
  TMPDIR=/tmp \
  PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
  /bin/bash -c \"$APP_EXEC\"
"
