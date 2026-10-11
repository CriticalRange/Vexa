#!/data/data/com.termux/files/usr/bin/bash
# ==============================================================================
# VEXA / Termux Native Root Chroot Stopper & Cleaner
# ==============================================================================

echo "[+] Chroot ve X11 süreçleri kapatılıyor..."
su -c '
ROOTFS="/data/data/com.termux/files/home/.andronix/distros/ubuntu/rootfs"

# Kill all processes inside chroot
fuser -k -9 "$ROOTFS" 2>/dev/null

# Clean unmounts
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
echo "[+] Tüm servisler durduruldu ve bağlantılar temizlendi."
