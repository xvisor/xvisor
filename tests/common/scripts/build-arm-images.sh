#!/bin/bash

function usage()
{
	echo "Usage:"
	echo " $0 [options]"
	echo "Options:"
	echo "     -h                       Display help or usage (Optional)"
	echo "     -a <arm_family>          Xvisor ARM architecture family (Mandatory)"
	echo "                                Allowed values:"
	echo "                                  v5, v6, v7, v7-ve, v8"
	echo "     -g <guest_type>          Xvisor Guest type (Mandatory)"
	echo "                                Allowed values:"
	echo "                                  realview-eb-mpcore"
	echo "                                  realview-pb-a8"
	echo "                                  versatilepb"
	echo "                                  vexpress-a9"
	echo "                                  vexpress-a15"
	echo "                                  virt-v7"
	echo "                                  virt-v8"
	echo "     -p <xvisor_source_path>  Xvisor source path (Optional)"
	echo "     -o <xvisor_output_path>  Xvisor output path (Optional)"
	echo "     -d <tarball_path>        Directory containing Linux and Busybox tarball (Optional)"
	echo "     -j <num_threads>         Number of threads for Make (Optional)"
	echo "     -l <linux_version>       Guest Linux version (Optional)"
	echo "     -b <busybox_version>     Guest Busybox version (Optional)"
	echo "     -i                       Only print build configuration (Optional)"
	echo "     -v <guest_output_path>   Guest Linux and Guest Busybox output path (Optional)"
	echo "     -x                       Only build Xvisor (Optional)"
	exit 1;
}

# Command line options
BUILD_ARM_FAMILY=
BUILD_GUEST_TYPE=
BUILD_NUM_THREADS=1
BUILD_XVISOR_SOURCE_PATH=`pwd`
BUILD_XVISOR_ONLY="no"
BUILD_XVISOR_OUTPUT_PATH=`pwd`/build
BUILD_TARBALL_PATH=`pwd`/build-targz
BUILD_GUEST_OUTPUT_PATH=`pwd`/build-guest
BUILD_LINUX_VERSION="4.1"
BUILD_BUSYBOX_VERSION="1.21.1"
BUILD_PRINT_CONFIG_ONLY="no"

# Derived options
BUILD_XVISOR_ARCH=
BUILD_XVISOR_CROSS_COMPILE=
BUILD_XVISOR_DEFCONFIG=
BUILD_XVISOR_TESTS_DIR=
BUILD_XVISOR_GUEST_DTS_BASENAME=
BUILD_XVISOR_BASIC_FIRMWARE_SOURCE_PATH=
BUILD_XVISOR_DISK_PATH=
BUILD_XVISOR_DISK_EXT2_PATH=
BUILD_LINUX_CPATCH="no"
BUILD_LINUX_ARCH=
BUILD_LINUX_CROSS_COMPILE=
BUILD_LINUX_TARBALL=
BUILD_LINUX_TARBALL_PATH=
BUILD_LINUX_TARBALL_URL=
BUILD_LINUX_OLDCONFIG_PATH=
BUILD_LINUX_SOURCE_PATH=
BUILD_LINUX_OUTPUT_PATH=
BUILD_LINUX_DTB_NAME=
BUILD_LINUX_DTS_PATH=
BUILD_BUSYBOX_CROSS_COMPILE=
BUILD_BUSYBOX_TARBALL=
BUILD_BUSYBOX_TARBALL_PATH=
BUILD_BUSYBOX_TARBALL_URL=
BUILD_BUSYBOX_OLDCONFIG_PATH=
BUILD_BUSYBOX_OUTPUT_PATH=
BUILD_BUSYBOX_ROOTFS_CPIO_PATH=
BUILD_BUSYBOX_ROOTFS_EXT2_PATH=

while getopts ":a:b:d:g:h:ij:l:o:p:v:x" o; do
	case "${o}" in
	a)
		BUILD_ARM_FAMILY=${OPTARG}
		;;
	b)
		BUILD_BUSYBOX_VERSION=${OPTARG}
		;;
	d)
		BUILD_TARBALL_PATH=${OPTARG}
		;;
	g)
		BUILD_GUEST_TYPE=${OPTARG}
		;;
	h)
		usage
		;;
	i)
		BUILD_PRINT_CONFIG_ONLY="yes"
		;;
	j)
		BUILD_NUM_THREADS=${OPTARG}
		;;
	l)
		BUILD_LINUX_VERSION=${OPTARG}
		;;
	o)
		BUILD_XVISOR_OUTPUT_PATH=${OPTARG}
		;;
	p)
		BUILD_XVISOR_SOURCE_PATH=${OPTARG}
		;;
	v)
		BUILD_GUEST_OUTPUT_PATH=${OPTARG}
		;;
	x)
		BUILD_XVISOR_ONLY="yes"
		;;
	*)
		usage
		;;
	esac
done
shift $((OPTIND-1))

if [ -z "${BUILD_ARM_FAMILY}" ]; then
	echo "Must specify ARM family"
	usage
fi

case "${BUILD_ARM_FAMILY}" in
v5)
	BUILD_XVISOR_ARCH="arm"
	BUILD_XVISOR_CROSS_COMPILE=arm-linux-gnueabi-
	BUILD_LINUX_CPATCH="yes"
	BUILD_LINUX_ARCH="arm"
	BUILD_LINUX_CROSS_COMPILE=arm-linux-gnueabi-
	BUILD_BUSYBOX_CROSS_COMPILE=arm-none-linux-gnueabi-
	;;
v6)
	BUILD_XVISOR_ARCH="arm"
	BUILD_XVISOR_CROSS_COMPILE=arm-linux-gnueabi-
	BUILD_LINUX_CPATCH="yes"
	BUILD_LINUX_ARCH="arm"
	BUILD_LINUX_CROSS_COMPILE=arm-linux-gnueabi-
	BUILD_BUSYBOX_CROSS_COMPILE=arm-none-linux-gnueabi-
	;;
v7)
	BUILD_XVISOR_ARCH="arm"
	BUILD_XVISOR_CROSS_COMPILE=arm-linux-gnueabi-
	BUILD_LINUX_CPATCH="yes"
	BUILD_LINUX_ARCH="arm"
	BUILD_LINUX_CROSS_COMPILE=arm-linux-gnueabi-
	BUILD_BUSYBOX_CROSS_COMPILE=arm-linux-gnueabi-
	;;
v7-ve)
	BUILD_XVISOR_ARCH="arm"
	BUILD_XVISOR_CROSS_COMPILE=arm-linux-gnueabi-
	BUILD_LINUX_CPATCH="no"
	BUILD_LINUX_ARCH="arm"
	BUILD_LINUX_CROSS_COMPILE=arm-linux-gnueabi-
	BUILD_BUSYBOX_CROSS_COMPILE=arm-linux-gnueabihf-
	;;
v8)
	BUILD_XVISOR_ARCH="arm"
	BUILD_XVISOR_CROSS_COMPILE=aarch64-linux-gnu-
	BUILD_LINUX_CPATCH="no"
	if [ "${BUILD_GUEST_TYPE}" == "virt-v8" ]; then
		BUILD_LINUX_ARCH="arm64"
		BUILD_LINUX_CROSS_COMPILE=aarch64-linux-gnu-
		BUILD_BUSYBOX_CROSS_COMPILE=aarch64-linux-gnu-
	fi
	if [ "${BUILD_GUEST_TYPE}" != "virt-v8" ]; then
		BUILD_LINUX_ARCH="arm"
		BUILD_LINUX_CROSS_COMPILE=arm-linux-gnueabi-
		BUILD_BUSYBOX_CROSS_COMPILE=arm-none-linux-gnueabi-
	fi
	;;
*)
	echo "Invalid ARM family"
	usage
	;;
esac

if [ -z "${BUILD_GUEST_TYPE}" ]; then
	echo "Must specify Guest type"
	usage
fi

case "${BUILD_GUEST_TYPE}" in
realview-eb-mpcore)
	if [ "${BUILD_ARM_FAMILY}" != "v6" ]; then
		echo "ARM family has to be v6 for ${BUILD_GUEST_TYPE}"
		usage
	fi
	BUILD_XVISOR_TESTS_DIR=arm32
	BUILD_XVISOR_GUEST_DTS_BASENAME=realview-eb-mpcore
	;;
realview-pb-a8)
	if [ "${BUILD_ARM_FAMILY}" == "v5" ]; then
		echo "ARM family cannot be v5 for ${BUILD_GUEST_TYPE}"
		usage
	fi
	if [ "${BUILD_ARM_FAMILY}" == "v6" ]; then
		echo "ARM family cannot be v6 for ${BUILD_GUEST_TYPE}"
		usage
	fi
	BUILD_XVISOR_TESTS_DIR=arm32
	BUILD_XVISOR_GUEST_DTS_BASENAME=realview-pb-a8
	;;
versatilepb)
	if [ "${BUILD_ARM_FAMILY}" != "v5" ]; then
		echo "ARM family has to be v5 for ${BUILD_GUEST_TYPE}"
		usage
	fi
	BUILD_XVISOR_TESTS_DIR=arm32
	BUILD_XVISOR_GUEST_DTS_BASENAME=versatilepb
	;;
vexpress-a9)
	if [ "${BUILD_ARM_FAMILY}" == "v5" ]; then
		echo "ARM family cannot be v5 for ${BUILD_GUEST_TYPE}"
		usage
	fi
	if [ "${BUILD_ARM_FAMILY}" == "v6" ]; then
		echo "ARM family cannot be v6 for ${BUILD_GUEST_TYPE}"
		usage
	fi
	BUILD_XVISOR_TESTS_DIR=arm32
	BUILD_XVISOR_GUEST_DTS_BASENAME=vexpress-a9x2
	BUILD_LINUX_DTB_NAME=vexpress-v2p-ca9.dtb
	BUILD_LINUX_DTS_PATH=${BUILD_XVISOR_SOURCE_PATH}/tests/arm32/vexpress-a9/linux/vexpress-v2p-ca9.dts
	;;
vexpress-a15)
	if [ "${BUILD_ARM_FAMILY}" == "v5" ]; then
		echo "ARM family cannot be v5 for ${BUILD_GUEST_TYPE}"
		usage
	fi
	if [ "${BUILD_ARM_FAMILY}" == "v6" ]; then
		echo "ARM family cannot be v6 for ${BUILD_GUEST_TYPE}"
		usage
	fi
	if [ "${BUILD_ARM_FAMILY}" == "v7" ]; then
		echo "ARM family cannot be v7 for ${BUILD_GUEST_TYPE}"
		usage
	fi
	BUILD_XVISOR_TESTS_DIR=arm32
	BUILD_XVISOR_GUEST_DTS_BASENAME=vexpress-a15x2
	BUILD_LINUX_DTB_NAME=vexpress-v2p-ca15-tc1.dtb
	BUILD_LINUX_DTS_PATH=${BUILD_XVISOR_SOURCE_PATH}/tests/arm32/vexpress-a15/linux/vexpress-v2p-ca15-tc1.dts
	;;
virt-v7)
	if [ "${BUILD_ARM_FAMILY}" == "v5" ]; then
		echo "ARM family cannot be v5 for ${BUILD_GUEST_TYPE}"
		usage
	fi
	if [ "${BUILD_ARM_FAMILY}" == "v6" ]; then
		echo "ARM family cannot be v6 for ${BUILD_GUEST_TYPE}"
		usage
	fi
	if [ "${BUILD_ARM_FAMILY}" == "v7" ]; then
		echo "ARM family cannot be v7 for ${BUILD_GUEST_TYPE}"
		usage
	fi
	BUILD_XVISOR_TESTS_DIR=arm32
	BUILD_XVISOR_GUEST_DTS_BASENAME=virt-v7x2
	BUILD_LINUX_DTB_NAME=virt-v7.dtb
	BUILD_LINUX_DTS_PATH=${BUILD_XVISOR_SOURCE_PATH}/tests/arm32/virt-v7/linux/virt-v7.dts
	;;
virt-v8)
	if [ "${BUILD_ARM_FAMILY}" != "v8" ]; then
		echo "ARM family has to be v8 for ${BUILD_GUEST_TYPE}"
		usage
	fi
	BUILD_XVISOR_TESTS_DIR=arm64
	BUILD_XVISOR_GUEST_DTS_BASENAME=virt-v8x2
	BUILD_LINUX_DTB_NAME=virt-v8.dtb
	BUILD_LINUX_DTS_PATH=${BUILD_XVISOR_SOURCE_PATH}/tests/arm64/virt-v8/linux/virt-v8.dts
	;;
*)
	echo "Invalid Guest type"
	usage
	;;
esac

if [ -z "${BUILD_XVISOR_SOURCE_PATH}" ]; then
	echo "Must specify Xvisor source path"
	usage
fi

if [ ! -d ${BUILD_XVISOR_SOURCE_PATH} ]; then
	echo "Xvisor source path does not exist"
	usage
fi

if [ -z "${BUILD_XVISOR_OUTPUT_PATH}" ]; then
	echo "Must specify xvisor output path"
	usage
fi

if [ "${BUILD_XVISOR_OUTPUT_PATH}" == "${BUILD_XVISOR_SOURCE_PATH}" ]; then
	echo "Use output path different from Xvisor source path"
	usage
fi

if [ -z "${BUILD_LINUX_VERSION}" ]; then
	echo "Must specify Linux version"
	usage
fi

if [ -z "${BUILD_BUSYBOX_VERSION}" ]; then
	echo "Must specify Busybox version"
	usage
fi

if [ -z "${BUILD_LINUX_DTB_NAME}" ]; then
	if [ ! -f ${BUILD_LINUX_DTS_PATH} ]; then
		echo "Linux DTS file not found"
		usage
	fi
fi

BUILD_XVISOR_DEFCONFIG="generic-${BUILD_ARM_FAMILY}-defconfig"
BUILD_XVISOR_BASIC_FIRMWARE_SOURCE_PATH=${BUILD_XVISOR_SOURCE_PATH}/tests/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/basic
BUILD_XVISOR_DISK_PATH=${BUILD_XVISOR_OUTPUT_PATH}/disk-${BUILD_GUEST_TYPE}
BUILD_XVISOR_DISK_EXT2_PATH=${BUILD_XVISOR_OUTPUT_PATH}/disk-${BUILD_GUEST_TYPE}.ext2
BUILD_LINUX_TARBALL="linux-${BUILD_LINUX_VERSION}.tar.gz"
BUILD_LINUX_TARBALL_PATH=${BUILD_TARBALL_PATH}/${BUILD_LINUX_TARBALL}
BUILD_LINUX_TARBALL_URL="https://www.kernel.org/pub/linux/kernel/v4.x/${BUILD_LINUX_TARBALL}"
BUILD_LINUX_SOURCE_PATH=${BUILD_GUEST_OUTPUT_PATH}/linux-${BUILD_LINUX_VERSION}
BUILD_LINUX_OUTPUT_PATH=${BUILD_GUEST_OUTPUT_PATH}/linux-${BUILD_GUEST_TYPE}
BUILD_LINUX_OLDCONFIG_PATH=${BUILD_XVISOR_SOURCE_PATH}/tests/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/linux/linux-${BUILD_LINUX_VERSION}_defconfig
BUILD_BUSYBOX_TARBALL="busybox-${BUILD_BUSYBOX_VERSION}.tar.bz2"
BUILD_BUSYBOX_TARBALL_PATH=${BUILD_TARBALL_PATH}/${BUILD_BUSYBOX_TARBALL}
BUILD_BUSYBOX_TARBALL_URL="http://busybox.net/downloads/${BUILD_BUSYBOX_TARBALL}"
BUILD_BUSYBOX_OLDCONFIG_PATH=${BUILD_XVISOR_SOURCE_PATH}/tests/common/busybox/busybox-${BUILD_BUSYBOX_VERSION}_defconfig
BUILD_BUSYBOX_OUTPUT_PATH=${BUILD_GUEST_OUTPUT_PATH}/busybox-${BUILD_GUEST_TYPE}
BUILD_BUSYBOX_ROOTFS_CPIO_PATH=${BUILD_GUEST_OUTPUT_PATH}/rootfs-${BUILD_GUEST_TYPE}.cpio
BUILD_BUSYBOX_ROOTFS_EXT2_PATH=${BUILD_GUEST_OUTPUT_PATH}/rootfs-${BUILD_GUEST_TYPE}.ext2

echo "=== Build configuration ==="
echo "arm_family = ${BUILD_ARM_FAMILY}"
echo "guest_type = ${BUILD_GUEST_TYPE}"
echo "guest_output_path = ${BUILD_GUEST_OUTPUT_PATH}"
echo "num_threads = ${BUILD_NUM_THREADS}"
echo "tarball_path = ${BUILD_TARBALL_PATH}"
echo "xvisor_arch = ${BUILD_XVISOR_ARCH}"
echo "xvisor_cross_compile = ${BUILD_XVISOR_CROSS_COMPILE}"
echo "xvisor_defconfig = ${BUILD_XVISOR_DEFCONFIG}"
echo "xvisor_source_path = ${BUILD_XVISOR_SOURCE_PATH}"
echo "xvisor_output_path = ${BUILD_XVISOR_OUTPUT_PATH}"
echo "xvisor_guest_dts_basename = ${BUILD_XVISOR_GUEST_DTS_BASENAME}"
echo "xvisor_disk_path = ${BUILD_XVISOR_DISK_PATH}"
echo "xvisor_disk_ext2_path = ${BUILD_XVISOR_DISK_EXT2_PATH}"
echo "xvisor_basic_firmware_source_path = ${BUILD_XVISOR_BASIC_FIRMWARE_SOURCE_PATH}"
echo "xvisor_only = ${BUILD_XVISOR_ONLY}"
echo "linux_version = ${BUILD_LINUX_VERSION}"
echo "linux_cpatch = ${BUILD_LINUX_CPATCH}"
echo "linux_arch = ${BUILD_LINUX_ARCH}"
echo "linux_cross_compile = ${BUILD_LINUX_CROSS_COMPILE}"
echo "linux_tarball = ${BUILD_LINUX_TARBALL}"
echo "linux_tarball_path = ${BUILD_LINUX_TARBALL_PATH}"
echo "linux_tarball_url = ${BUILD_LINUX_TARBALL_URL}"
echo "linux_oldconfig_path = ${BUILD_LINUX_OLDCONFIG_PATH}"
echo "linux_source_path = ${BUILD_LINUX_SOURCE_PATH}"
echo "linux_output_path = ${BUILD_LINUX_OUTPUT_PATH}"
echo "linux_dtb_name = ${BUILD_LINUX_DTB_NAME}"
echo "linux_dts_path = ${BUILD_LINUX_DTS_PATH}"
echo "busybox_version = ${BUILD_BUSYBOX_VERSION}"
echo "busybox_cross_compile = ${BUILD_BUSYBOX_CROSS_COMPILE}"
echo "busybox_tarball = ${BUILD_BUSYBOX_TARBALL}"
echo "busybox_tarball_path = ${BUILD_BUSYBOX_TARBALL_PATH}"
echo "busybox_tarball_url = ${BUILD_BUSYBOX_TARBALL_URL}"
echo "busybox_oldconfig_path = ${BUILD_BUSYBOX_OLDCONFIG_PATH}"
echo "busybox_output_path = ${BUILD_BUSYBOX_OUTPUT_PATH}"
echo "busybox_rootfs_cpio_path = ${BUILD_BUSYBOX_ROOTFS_CPIO_PATH}"
echo "busybox_rootfs_ext2_path = ${BUILD_BUSYBOX_ROOTFS_EXT2_PATH}"

if [ "${BUILD_PRINT_CONFIG_ONLY}" == "yes" ]; then
	exit 0
fi

echo "=== Ensure Xvisor output path is present ==="
mkdir -p ${BUILD_XVISOR_OUTPUT_PATH}

echo "=== Build Xvisor and DTBs ==="
export ARCH=${BUILD_XVISOR_ARCH}
export CROSS_COMPILE=${BUILD_XVISOR_CROSS_COMPILE}
if [ ! -d ${BUILD_XVISOR_OUTPUT_PATH}/openconf ]; then
	make -C ${BUILD_XVISOR_SOURCE_PATH} O=${BUILD_XVISOR_OUTPUT_PATH} ARCH=${BUILD_XVISOR_ARCH} ${BUILD_XVISOR_DEFCONFIG}
fi
make -C ${BUILD_XVISOR_SOURCE_PATH} O=${BUILD_XVISOR_OUTPUT_PATH} ARCH=${BUILD_XVISOR_ARCH} -j ${BUILD_NUM_THREADS}
make -C ${BUILD_XVISOR_SOURCE_PATH} O=${BUILD_XVISOR_OUTPUT_PATH} ARCH=${BUILD_XVISOR_ARCH} -j ${BUILD_NUM_THREADS} dtbs

echo "=== Build Xvisor Basic Firmware ==="
export ARCH=${BUILD_XVISOR_ARCH}
export CROSS_COMPILE=${BUILD_LINUX_CROSS_COMPILE}
make -C ${BUILD_XVISOR_BASIC_FIRMWARE_SOURCE_PATH} O=${BUILD_XVISOR_OUTPUT_PATH} -j ${BUILD_NUM_THREADS}

if [ "${BUILD_XVISOR_ONLY}" == "yes" ]; then
	exit 0
fi

echo "=== Fetching Linux and Busybox tarballs ==="
mkdir -p ${BUILD_TARBALL_PATH}
if [ ! -f ${BUILD_LINUX_TARBALL_PATH} ]; then
	wget -P ${BUILD_TARBALL_PATH}/ ${BUILD_LINUX_TARBALL_URL}
fi
if [ ! -f ${BUILD_BUSYBOX_TARBALL_PATH} ]; then
	wget -P ${BUILD_TARBALL_PATH}/ ${BUILD_BUSYBOX_TARBALL_URL}
fi

echo "=== Untaring Linux and Busybox tarballs ==="
mkdir -p ${BUILD_GUEST_OUTPUT_PATH}
if [ ! -d ${BUILD_LINUX_SOURCE_PATH} ]; then
	tar -C ${BUILD_GUEST_OUTPUT_PATH} -xvf ${BUILD_LINUX_TARBALL_PATH}
fi
if [ ! -d ${BUILD_BUSYBOX_OUTPUT_PATH} ]; then
	tar -C ${BUILD_GUEST_OUTPUT_PATH} -xvf ${BUILD_BUSYBOX_TARBALL_PATH}
	mv ${BUILD_GUEST_OUTPUT_PATH}/busybox-${BUILD_BUSYBOX_VERSION} ${BUILD_BUSYBOX_OUTPUT_PATH}
fi

echo "=== Configure and Build Linux ==="
export ARCH=${BUILD_LINUX_ARCH}
export CROSS_COMPILE=${BUILD_LINUX_CROSS_COMPILE}
mkdir -p ${BUILD_LINUX_OUTPUT_PATH}
if [ ! -f ${BUILD_LINUX_OUTPUT_PATH}/.config ]; then
	cp -f ${BUILD_LINUX_OLDCONFIG_PATH} ${BUILD_LINUX_OUTPUT_PATH}/.config
	make ARCH=${BUILD_LINUX_ARCH} -C ${BUILD_LINUX_SOURCE_PATH} O=${BUILD_LINUX_OUTPUT_PATH} oldconfig
fi
if [ ! -f ${BUILD_LINUX_OUTPUT_PATH}/vmlinux ]; then
	make ARCH=${BUILD_LINUX_ARCH} -C ${BUILD_LINUX_SOURCE_PATH} O=${BUILD_LINUX_OUTPUT_PATH} -j ${BUILD_NUM_THREADS} Image
	cp -f ${BUILD_LINUX_OUTPUT_PATH}/arch/${BUILD_LINUX_ARCH}/boot/Image ${BUILD_LINUX_OUTPUT_PATH}/arch/${BUILD_LINUX_ARCH}/boot/Image.orig
	if [ "${BUILD_LINUX_CPATCH}" == "yes" ]; then
		${BUILD_XVISOR_SOURCE_PATH}/arch/arm/cpu/arm32/elf2cpatch.py -f ${BUILD_LINUX_OUTPUT_PATH}/vmlinux | ${BUILD_XVISOR_OUTPUT_PATH}/tools/cpatch/cpatch32 ${BUILD_LINUX_OUTPUT_PATH}/vmlinux 0
		make ARCH=${BUILD_LINUX_ARCH} -C ${BUILD_LINUX_SOURCE_PATH} O=${BUILD_LINUX_OUTPUT_PATH} Image
	fi
fi

echo "=== Configure and Build Busybox ==="
export ARCH=${BUILD_LINUX_ARCH}
export CROSS_COMPILE=${BUILD_BUSYBOX_CROSS_COMPILE}
if [ ! -f ${BUILD_BUSYBOX_OUTPUT_PATH}/.config ]; then
	cp -f ${BUILD_BUSYBOX_OLDCONFIG_PATH} ${BUILD_BUSYBOX_OUTPUT_PATH}/.config
	make -C ${BUILD_BUSYBOX_OUTPUT_PATH} oldconfig
fi
if [ ! -f ${BUILD_BUSYBOX_OUTPUT_PATH}/_install/bin/busybox ]; then
	make -C ${BUILD_BUSYBOX_OUTPUT_PATH} -j ${BUILD_NUM_THREADS} install
	mkdir -p ${BUILD_BUSYBOX_OUTPUT_PATH}/_install/etc/init.d
	mkdir -p ${BUILD_BUSYBOX_OUTPUT_PATH}/_install/dev
	mkdir -p ${BUILD_BUSYBOX_OUTPUT_PATH}/_install/proc
	mkdir -p ${BUILD_BUSYBOX_OUTPUT_PATH}/_install/sys
	ln -sf /sbin/init ${BUILD_BUSYBOX_OUTPUT_PATH}/_install/init
	cp -f ${BUILD_XVISOR_SOURCE_PATH}/tests/common/busybox/fstab ${BUILD_BUSYBOX_OUTPUT_PATH}/_install/etc/fstab
	cp -f ${BUILD_XVISOR_SOURCE_PATH}/tests/common/busybox/rcS ${BUILD_BUSYBOX_OUTPUT_PATH}/_install/etc/init.d/rcS
	cp -f ${BUILD_XVISOR_SOURCE_PATH}/tests/common/busybox/motd ${BUILD_BUSYBOX_OUTPUT_PATH}/_install/etc/motd
fi

echo "=== Create Busybox Rootfs Images ==="
if [ ! -f ${BUILD_BUSYBOX_ROOTFS_CPIO_PATH} ]; then
	cd ${BUILD_BUSYBOX_OUTPUT_PATH}/_install; find ./ | cpio -o -H newc > ${BUILD_BUSYBOX_ROOTFS_CPIO_PATH}; cd -
fi
if [ ! -f ${BUILD_BUSYBOX_ROOTFS_EXT2_PATH} ]; then
	genext2fs -b 6500 -N 1024 -U -d ${BUILD_BUSYBOX_OUTPUT_PATH}/_install ${BUILD_BUSYBOX_ROOTFS_EXT2_PATH}
fi

echo "=== Create Xvisor Disk Image ==="
if [ ! -d ${BUILD_XVISOR_DISK_PATH} ]; then
	mkdir -p ${BUILD_XVISOR_DISK_PATH}
	mkdir -p ${BUILD_XVISOR_DISK_PATH}/tmp
	mkdir -p ${BUILD_XVISOR_DISK_PATH}/system
	cp -f ${BUILD_XVISOR_SOURCE_PATH}/docs/banner/roman.txt ${BUILD_XVISOR_DISK_PATH}/system/banner.txt
	cp -f ${BUILD_XVISOR_SOURCE_PATH}/docs/logo/xvisor_logo_name.ppm ${BUILD_XVISOR_DISK_PATH}/system/logo.ppm
	mkdir -p ${BUILD_XVISOR_DISK_PATH}/images/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}
	${BUILD_XVISOR_OUTPUT_PATH}/tools/dtc/bin/dtc -I dts -O dtb -o ${BUILD_XVISOR_DISK_PATH}/images/${BUILD_XVISOR_TESTS_DIR}/${BUILD_XVISOR_GUEST_DTS_BASENAME}.dtb ${BUILD_XVISOR_SOURCE_PATH}/tests/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/${BUILD_XVISOR_GUEST_DTS_BASENAME}.dts
	if [ "${BUILD_LINUX_CPATCH}" == "yes" ]; then
		cp -f ${BUILD_XVISOR_OUTPUT_PATH}/tests/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/basic/firmware.bin.patched ${BUILD_XVISOR_DISK_PATH}/images/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/firmware.bin
	fi
	if [ "${BUILD_LINUX_CPATCH}" != "yes" ]; then
		cp -f ${BUILD_XVISOR_OUTPUT_PATH}/tests/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/basic/firmware.bin ${BUILD_XVISOR_DISK_PATH}/images/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/firmware.bin
	fi
	cp -f ${BUILD_XVISOR_SOURCE_PATH}/tests/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/linux/nor_flash.list ${BUILD_XVISOR_DISK_PATH}/images/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/nor_flash.list
	cp -f ${BUILD_XVISOR_SOURCE_PATH}/tests/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/linux/cmdlist ${BUILD_XVISOR_DISK_PATH}/images/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/cmdlist
	cp -f ${BUILD_LINUX_OUTPUT_PATH}/arch/${BUILD_LINUX_ARCH}/boot/Image ${BUILD_XVISOR_DISK_PATH}/images/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/Image
	if [ ! -z "${BUILD_LINUX_DTB_NAME}" ]; then
		${BUILD_XVISOR_OUTPUT_PATH}/tools/dtc/bin/dtc -I dts -O dtb -p 0x800 -o ${BUILD_XVISOR_DISK_PATH}/images/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/${BUILD_LINUX_DTB_NAME} ${BUILD_LINUX_DTS_PATH}
	fi
	cp -f ${BUILD_BUSYBOX_ROOTFS_CPIO_PATH} ${BUILD_XVISOR_DISK_PATH}/images/${BUILD_XVISOR_TESTS_DIR}/rootfs.img
fi
if [ ! -f ${BUILD_XVISOR_DISK_EXT2_PATH} ]; then
	genext2fs -B 1024 -b 16384 -d ${BUILD_XVISOR_DISK_PATH} ${BUILD_XVISOR_DISK_EXT2_PATH}
fi

export ARCH=${BUILD_XVISOR_ARCH}
export CROSS_COMPILE=${BUILD_XVISOR_CROSS_COMPILE}
