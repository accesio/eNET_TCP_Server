#!/bin/bash

PHASE=1

led_flash () {
	echo $1
	for (( i = 1; i<=$1; i++ ))
       	do
		echo 1 > /sys/devices/platform/leds/leds/cpu-0\ activity/brightness
		sleep 0.1
		echo 0 > /sys/devices/platform/leds/leds/cpu-0\ activity/brightness
		sleep 0.1
	done
}

led_set () {
	echo $1 > /sys/devices/platform/leds/leds/cpu-0\ activity/brightness
}

MOUNT_POINT=$(lsblk -o mountpoints | grep mmc)
MMC_DEV="${MOUNT_POINT#/run/media/mmcblk}"
MMC_DEV="${MMC_DEV:0:1}"
MMC_DEV=0

echo /dev/mmcblk"${MMC_DEV}"

echo FLASHING BOOTLOADER
led_flash $PHASE
((PHASE++))

echo 0 > /sys/block/mmcblk${MMC_DEV}boot0/force_ro
dd if=/opt/tiboot3.bin of=/dev/mmcblk"${MMC_DEV}"boot0 seek=0
dd if=/opt/tispl.bin of=/dev/mmcblk"${MMC_DEV}"boot0 seek=2048
dd if=/opt/u-boot.img of=/dev/mmcblk"${MMC_DEV}"boot0 seek=6144
[ $? -eq 0 ] && echo DONE || exit

echo WRITING PARTITION TABLE
led_flash $PHASE
((PHASE++))

umount "${MOUNT_POINT}"

sfdisk /dev/mmcblk"${MMC_DEV}" << EOF
label: dos
label-id: 0x7a076551
device: /dev/mmcblk"${MMC_DEV}"
unit: sectors
sector-size: 512

/dev/mmcblk"${MMC_DEV}"p1 : start=        2048, size=    15267840, type=83
EOF
[ $? -eq 0 ] && echo DONE || exit

echo SETTING HW RESET ENABLE
led_flash $PHASE
((PHASE++))

mmc hwreset enable /dev/mmcblk"${MMC_DEV}"
#Can't check hwreset because it will error out if it's already been set, and
#we want to be able to reflash emmc
[ $? -eq 0 ] && echo DONE || echo ALREADY_SET

echo SETTING EMMC PARTITION_CONFIG
led_flash $PHASE
((PHASE++))

mmc bootpart enable 1 1 /dev/mmcblk"${MMC_DEV}"
[ $? -eq 0 ] && echo DONE || exit

echo SETTING EMMC BOOT_BUS_CONDITIONS
led_flash $PHASE
((PHASE++))

mmc bootbus set single_backward x1 x8 /dev/mmcblk"${MMC_DEV}"
[ $? -eq 0 ] && echo DONE || exit

echo CREATING ext4 FILESYSTEM
led_flash $PHASE
((PHASE++))
yes | mkfs.ext4 /dev/mmcblk"${MMC_DEV}"p1
[ $? -eq 0 ] && echo DONE || exit

echo WRITING ROOTFS
led_flash $PHASE
((PHASE++))

if mountpoint -q /run/media/mmcblk0p1; then
     umount /run/media/mmcblk0p1
fi

mount -t ext4 /dev/mmcblk0p1 /opt/mnt
[ $? -eq 0 ] && echo mounted || exit

tar -xf /opt/rootfs.tar.xz -C /opt/mnt
[ $? -eq 0 ] && echo written || exit

umount /opt/mnt
[ $? -eq 0 ] && echo umounted || exit

led_set 1
