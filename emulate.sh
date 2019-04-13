#!/bin/sh
dev_table=${GUMSTIX_DEVICE_TABLE-${EC535}/gumstix/device_table.txt}
u_boot_bin=${GUMSTIX_U_BOOT_BIN-u-boot.bin}
uimage=${GUMSTIX_UIMAGE-uImage}
rootfs_dir=${GUMSTIX_ROOTFS_DIR-rootfs}
bundled_rootfs=rootfs_gumstix.jffs2

if [ ! -d "${rootfs_dir}" ]
then
	1>&2 echo "Copy a rootfs directory to ${rootfs_dir} or set GUMSTIX_ROOTFS_DIR to point to one"
	exit 1
fi

if [ ! -f "${u_boot_bin}" ]
then
	1>&2 echo "Copy a u-boot binary file to ${u_boot_bin} or set GUMSTIX_U_BOOT_BIN to point to one"
	exit 1
fi

if [ ! -f "${uimage}" ]
then
	1>&2 echo "Copy a uImage file to ${uimage} or set GUMSTIX_UIMAGE to point to one"
	exit 1
fi

rm -f flash

make -C km clean
if make -C km EMULATION_KERNEL=1
then
	:
else
	1>&2 echo "Failed to build the kernel module. Aborting!"
	exit 1
fi

#make -C ul clean
#if make -C ul EMULATION_KERNEL=1
#then
#	:
#else
#	1>&2 echo "Failed to build counterstat. Aborting!"
#	exit 1
#fi

rm -r "${rootfs_dir}/home"
mkdir "${rootfs_dir}/home"
cp km/DMGturret.ko "${rootfs_dir}/home"
#cp ul/counterstat "${rootfs_dir}/home"

if mkfs.jffs2 -l -U -e 128KiB -d "${rootfs_dir}" -D "${dev_table}" sumtool -e 128KiB -o "${bundled_rootfs}" && \
	dd of=flash bs=1k count=16k if=/dev/zero && \
	dd of=flash bs=1k conv=notrunc if="${u_boot_bin}" && \
	dd of=flash bs=1k conv=notrunc seek=256 if="${bundled_rootfs}" && \
	dd of=flash bs=1k conv=notrunc seek=31744 if="${uimage}"
then
	echo "Bundled flash!"
else
	1>&2 echo "Failed to bundle flash. Aborting!"
	exit 1
fi

qemu-system-arm -M verdex -pflash flash -monitor null -nographic -m 289
