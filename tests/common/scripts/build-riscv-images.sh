#!/bin/bash

function usage()
{
	echo "Usage:"
	echo " $0 [options]"
	echo "Options:"
	echo "     -h                       Display help or usage (Optional)"
	echo "     -a <riscv_xlen>          Xvisor RISC-V architecture XLEN (Mandatory)"
	echo "                                Allowed values:"
	echo "                                  32b, 64b"
	echo "     -g <guest_type>          Xvisor Guest type (Mandatory)"
	echo "                                Allowed values:"
	echo "                                  virt32"
	echo "                                  virt64"
	echo "     -s <guest_xscript>       Xvisor Guest creation script (Mandatory)"
	echo "     -p <xvisor_source_path>  Xvisor source path (Optional)"
	echo "     -o <build_output_path>   Build output path (Optional)"
	echo "     -i <build_install_path>  Build install path (Optional)"
	echo "     -d <tarball_path>        Directory containing Linux and Busybox tarball (Optional)"
	echo "     -j <num_threads>         Number of threads for Make (Optional)"
	echo "     -l <linux_version>       Guest Linux version (Optional)"
	echo "     -b <busybox_version>     Guest Busybox version (Optional)"
	echo "     -v                       Only print build configuration (Optional)"
	echo "     -x                       Only build Xvisor (Optional)"
	echo "     -q <xvisor_cross_compile>  Cross compile prefix for Xvisor (Optional)"
	echo "     -y <linux_cross_compile>   Cross compile prefix for Linux (Optional)"
	echo "     -z <busybox_cross_compile> Cross compile prefix for Busybox (Optional)"
	exit 1;
}

# Command line options
BUILD_RISCV_XLEN=
BUILD_GUEST_TYPE=
BUILD_GUEST_XSCRIPT=
BUILD_NUM_THREADS=1
BUILD_OUTPUT_PATH=`pwd`/build
BUILD_INSTALL_PATH=`pwd`/install
BUILD_XVISOR_SOURCE_PATH=`pwd`
BUILD_XVISOR_ONLY="no"
BUILD_XVISOR_OUTPUT_PATH=`pwd`/build/xvisor
BUILD_TARBALL_PATH=`pwd`/tarball
BUILD_GUEST_OUTPUT_PATH=`pwd`/build/guest
BUILD_LINUX_VERSION="5.4.6"
BUILD_BUSYBOX_VERSION="1.31.1"
BUILD_PRINT_CONFIG_ONLY="no"

# Derived options
BUILD_XVISOR_ARCH=
BUILD_XVISOR_CROSS_COMPILE=
BUILD_XVISOR_DEFCONFIG=
BUILD_XVISOR_TESTS_DIR=
BUILD_XVISOR_GUEST_DTS_BASENAME=
BUILD_XVISOR_BASIC_FIRMWARE_SOURCE_PATH=
BUILD_XVISOR_LINUX_DTS_PATH=
BUILD_XVISOR_DISK_LINUX_PATH=
BUILD_XVISOR_DISK_LINUX_EXT2_PATH=
BUILD_LINUX_ARCH=
BUILD_LINUX_CROSS_COMPILE=
BUILD_LINUX_TARBALL=
BUILD_LINUX_TARBALL_PATH=
BUILD_LINUX_TARBALL_URL=
BUILD_LINUX_DEFCONFIG=
BUILD_LINUX_DEFCONFIG_EXTRA=
BUILD_LINUX_SOURCE_PATH=
BUILD_LINUX_OUTPUT_PATH=
BUILD_LINUX_DTB_NAME=
BUILD_BUSYBOX_CROSS_COMPILE=
BUILD_BUSYBOX_TARBALL=
BUILD_BUSYBOX_TARBALL_PATH=
BUILD_BUSYBOX_TARBALL_URL=
BUILD_BUSYBOX_OLDCONFIG_PATH=
BUILD_BUSYBOX_OUTPUT_PATH=
BUILD_BUSYBOX_ROOTFS_CPIO_PATH=
BUILD_BUSYBOX_ROOTFS_EXT2_PATH=

while getopts ":a:b:d:g:s:h:vj:l:i:o:p:q:xy:z:" o; do
	case "${o}" in
	a)
		BUILD_RISCV_XLEN=${OPTARG}
		;;
	b)
		BUILD_BUSYBOX_VERSION=${OPTARG}
		;;
	z)
		BUILD_BUSYBOX_CROSS_COMPILE=${OPTARG}
		;;
	d)
		BUILD_TARBALL_PATH=${OPTARG}
		;;
	g)
		BUILD_GUEST_TYPE=${OPTARG}
		;;
	s)
		BUILD_GUEST_XSCRIPT=${OPTARG}
		;;
	h)
		usage
		;;
	v)
		BUILD_PRINT_CONFIG_ONLY="yes"
		;;
	j)
		BUILD_NUM_THREADS=${OPTARG}
		;;
	l)
		BUILD_LINUX_VERSION=${OPTARG}
		;;
	y)
		BUILD_LINUX_CROSS_COMPILE=${OPTARG}
		;;
	i)
		BUILD_INSTALL_PATH=${OPTARG}
		;;
	o)
		BUILD_OUTPUT_PATH=${OPTARG}
		;;
	p)
		BUILD_XVISOR_SOURCE_PATH=${OPTARG}
		;;
	q)
		BUILD_XVISOR_CROSS_COMPILE=${OPTARG}
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

if [ -z "${BUILD_RISCV_XLEN}" ]; then
	echo "Must specify RISC-V XLEN"
	usage
fi

case "${BUILD_RISCV_XLEN}" in
32b)
	BUILD_XVISOR_ARCH="riscv"
	BUILD_XVISOR_CROSS_COMPILE_PREFERRED=riscv32-unknown-linux-gnu-
	BUILD_LINUX_ARCH="riscv"
	BUILD_LINUX_CROSS_COMPILE_PREFERRED=riscv32-unknown-linux-gnu-
	BUILD_BUSYBOX_CROSS_COMPILE_PREFERRED=riscv32-unknown-linux-gnu-
	;;
64b)
	BUILD_XVISOR_ARCH="riscv"
	BUILD_XVISOR_CROSS_COMPILE_PREFERRED=riscv64-linux-
	BUILD_LINUX_ARCH="riscv"
	if [ "${BUILD_GUEST_TYPE}" == "virt32" ]; then
		BUILD_LINUX_CROSS_COMPILE_PREFERRED=riscv32-unknown-linux-gnu-
		BUILD_BUSYBOX_CROSS_COMPILE_PREFERRED=riscv32-unknown-linux-gnu-
	else
		BUILD_LINUX_CROSS_COMPILE_PREFERRED=riscv64-linux-
		BUILD_BUSYBOX_CROSS_COMPILE_PREFERRED=riscv64-linux-
	fi
	;;
*)
	echo "Invalid RISC-V XLEN"
	usage
	;;
esac

if [ -z "${BUILD_XVISOR_CROSS_COMPILE}" ]; then
	BUILD_XVISOR_CROSS_COMPILE=${BUILD_XVISOR_CROSS_COMPILE_PREFERRED}
fi

if [ -z "${BUILD_LINUX_CROSS_COMPILE}" ]; then
	BUILD_LINUX_CROSS_COMPILE=${BUILD_LINUX_CROSS_COMPILE_PREFERRED}
fi

if [ -z "${BUILD_BUSYBOX_CROSS_COMPILE}" ]; then
	BUILD_BUSYBOX_CROSS_COMPILE=${BUILD_BUSYBOX_CROSS_COMPILE_PREFERRED}
fi

if [ -z "${BUILD_GUEST_TYPE}" ]; then
	echo "Must specify Guest type"
	usage
fi

case "${BUILD_GUEST_TYPE}" in
virt32)
	BUILD_XVISOR_TESTS_DIR=riscv
	BUILD_XVISOR_GUEST_DTS_BASENAME=virt32-guest
	BUILD_LINUX_DEFCONFIG=rv32_defconfig
	BUILD_LINUX_DEFCONFIG_EXTRA=${BUILD_XVISOR_SOURCE_PATH}/tests/riscv/virt32/linux/linux_extra.config
	BUILD_LINUX_DTB_NAME=virt32.dtb
	BUILD_XVISOR_LINUX_DTS_PATH=${BUILD_XVISOR_SOURCE_PATH}/tests/riscv/virt32/linux/virt32.dts
	;;
virt64)
	if [ "${BUILD_RISCV_XLEN}" != "64b" ]; then
		echo "RISC-V XLEN should be 64b for ${BUILD_GUEST_TYPE}"
		usage
	fi
	BUILD_XVISOR_TESTS_DIR=riscv
	BUILD_XVISOR_GUEST_DTS_BASENAME=virt64-guest
	BUILD_LINUX_DEFCONFIG=defconfig
	BUILD_LINUX_DEFCONFIG_EXTRA=${BUILD_XVISOR_SOURCE_PATH}/tests/riscv/virt64/linux/linux_extra.config
	BUILD_LINUX_DTB_NAME=virt64.dtb
	BUILD_XVISOR_LINUX_DTS_PATH=${BUILD_XVISOR_SOURCE_PATH}/tests/riscv/virt64/linux/virt64.dts
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

if [ ! -f ${BUILD_XVISOR_SOURCE_PATH}/tests/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/xscript/${BUILD_GUEST_XSCRIPT}.xscript ]; then
	echo "Xvisor Guest creation script does not exist"
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

if [ ! -z "${BUILD_XVISOR_LINUX_DTS_PATH}" ]; then
	if [ ! -f ${BUILD_XVISOR_LINUX_DTS_PATH} ]; then
		echo "Linux DTS file not found"
		usage
	fi
fi

BUILD_GUEST_OUTPUT_PATH=${BUILD_OUTPUT_PATH}/guest
BUILD_GUEST_INSTALL_PATH=${BUILD_INSTALL_PATH}/guest/${BUILD_GUEST_TYPE}
BUILD_GUEST_BASIC_INSTALL_PATH=${BUILD_GUEST_INSTALL_PATH}/basic
BUILD_GUEST_LINUX_INSTALL_PATH=${BUILD_GUEST_INSTALL_PATH}/linux-${BUILD_LINUX_VERSION}
BUILD_XVISOR_DEFCONFIG="generic-${BUILD_RISCV_XLEN}-defconfig"
BUILD_XVISOR_OUTPUT_PATH=${BUILD_OUTPUT_PATH}
BUILD_XVISOR_INSTALL_PATH=${BUILD_INSTALL_PATH}/xvisor
BUILD_XVISOR_BASIC_FIRMWARE_SOURCE_PATH=${BUILD_XVISOR_SOURCE_PATH}/tests/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/basic
BUILD_XVISOR_GUEST_DTS_PATH=${BUILD_XVISOR_SOURCE_PATH}/tests/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/${BUILD_XVISOR_GUEST_DTS_BASENAME}.dts
BUILD_XVISOR_DISK_BASIC_PATH=${BUILD_GUEST_OUTPUT_PATH}/disk-basic-${BUILD_GUEST_XSCRIPT}
BUILD_XVISOR_DISK_BASIC_EXT2_PATH=${BUILD_GUEST_OUTPUT_PATH}/disk-basic-${BUILD_GUEST_XSCRIPT}.ext2
BUILD_XVISOR_DISK_LINUX_PATH=${BUILD_GUEST_OUTPUT_PATH}/disk-linux-${BUILD_LINUX_VERSION}-${BUILD_GUEST_XSCRIPT}
BUILD_XVISOR_DISK_LINUX_EXT2_PATH=${BUILD_GUEST_OUTPUT_PATH}/disk-linux-${BUILD_LINUX_VERSION}-${BUILD_GUEST_XSCRIPT}.ext2
BUILD_LINUX_TARBALL="linux-${BUILD_LINUX_VERSION}.tar.xz"
BUILD_LINUX_TARBALL_PATH=${BUILD_TARBALL_PATH}/${BUILD_LINUX_TARBALL}
BUILD_LINUX_TARBALL_URL="https://www.kernel.org/pub/linux/kernel/v4.x/${BUILD_LINUX_TARBALL}"
BUILD_LINUX_SOURCE_PATH=${BUILD_GUEST_OUTPUT_PATH}/linux-${BUILD_LINUX_VERSION}
BUILD_LINUX_OUTPUT_PATH=${BUILD_GUEST_OUTPUT_PATH}/linux-${BUILD_LINUX_VERSION}-${BUILD_GUEST_TYPE}
BUILD_BUSYBOX_TARBALL="busybox-${BUILD_BUSYBOX_VERSION}.tar.bz2"
BUILD_BUSYBOX_TARBALL_PATH=${BUILD_TARBALL_PATH}/${BUILD_BUSYBOX_TARBALL}
BUILD_BUSYBOX_TARBALL_URL="http://busybox.net/downloads/${BUILD_BUSYBOX_TARBALL}"
BUILD_BUSYBOX_OLDCONFIG_PATH=${BUILD_XVISOR_SOURCE_PATH}/tests/common/busybox/busybox-${BUILD_BUSYBOX_VERSION}_defconfig
BUILD_BUSYBOX_OUTPUT_PATH=${BUILD_GUEST_OUTPUT_PATH}/busybox-${BUILD_GUEST_TYPE}
BUILD_BUSYBOX_ROOTFS_CPIO_PATH=${BUILD_GUEST_OUTPUT_PATH}/rootfs-${BUILD_GUEST_TYPE}.cpio
BUILD_BUSYBOX_ROOTFS_EXT2_PATH=${BUILD_GUEST_OUTPUT_PATH}/rootfs-${BUILD_GUEST_TYPE}.ext2

echo "=== Build configuration ==="
echo "riscv_xlen = ${BUILD_RISCV_XLEN}"
echo "output_path = ${BUILD_OUTPUT_PATH}"
echo "install_path = ${BUILD_INSTALL_PATH}"
echo "guest_type = ${BUILD_GUEST_TYPE}"
echo "guest_xscript = ${BUILD_GUEST_XSCRIPT}.xscript"
echo "guest_output_path = ${BUILD_GUEST_OUTPUT_PATH}"
echo "guest_install_path = ${BUILD_GUEST_INSTALL_PATH}"
echo "guest_basic_install_path = ${BUILD_GUEST_BASIC_INSTALL_PATH}"
echo "guest_linux_install_path = ${BUILD_GUEST_LINUX_INSTALL_PATH}"
echo "num_threads = ${BUILD_NUM_THREADS}"
echo "tarball_path = ${BUILD_TARBALL_PATH}"
echo "xvisor_arch = ${BUILD_XVISOR_ARCH}"
echo "xvisor_cross_compile = ${BUILD_XVISOR_CROSS_COMPILE}"
echo "xvisor_defconfig = ${BUILD_XVISOR_DEFCONFIG}"
echo "xvisor_source_path = ${BUILD_XVISOR_SOURCE_PATH}"
echo "xvisor_output_path = ${BUILD_XVISOR_OUTPUT_PATH}"
echo "xvisor_install_path = ${BUILD_XVISOR_INSTALL_PATH}"
echo "xvisor_guest_dtb = ${BUILD_XVISOR_GUEST_DTS_BASENAME}.dtb"
echo "xvisor_guest_dts_path = ${BUILD_XVISOR_GUEST_DTS_PATH}"
echo "xvisor_linux_dts_path = ${BUILD_XVISOR_LINUX_DTS_PATH}"
echo "xvisor_disk_basic_path = ${BUILD_XVISOR_DISK_BASIC_PATH}"
echo "xvisor_disk_basic_ext2_path = ${BUILD_XVISOR_DISK_BASIC_EXT2_PATH}"
echo "xvisor_disk_linux_path = ${BUILD_XVISOR_DISK_LINUX_PATH}"
echo "xvisor_disk_linux_ext2_path = ${BUILD_XVISOR_DISK_LINUX_EXT2_PATH}"
echo "xvisor_basic_firmware_source_path = ${BUILD_XVISOR_BASIC_FIRMWARE_SOURCE_PATH}"
echo "xvisor_only = ${BUILD_XVISOR_ONLY}"
echo "linux_version = ${BUILD_LINUX_VERSION}"
echo "linux_arch = ${BUILD_LINUX_ARCH}"
echo "linux_cross_compile = ${BUILD_LINUX_CROSS_COMPILE}"
echo "linux_tarball = ${BUILD_LINUX_TARBALL}"
echo "linux_tarball_path = ${BUILD_LINUX_TARBALL_PATH}"
echo "linux_tarball_url = ${BUILD_LINUX_TARBALL_URL}"
echo "linux_defconfig = ${BUILD_LINUX_DEFCONFIG}"
echo "linux_defconfig_extra = ${BUILD_LINUX_DEFCONFIG_EXTRA}"
echo "linux_source_path = ${BUILD_LINUX_SOURCE_PATH}"
echo "linux_output_path = ${BUILD_LINUX_OUTPUT_PATH}"
echo "linux_dtb_name = ${BUILD_LINUX_DTB_NAME}"
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

echo "=== Build Xvisor Basic Firmware ==="
export ARCH=${BUILD_XVISOR_ARCH}
export CROSS_COMPILE=${BUILD_LINUX_CROSS_COMPILE}
make -C ${BUILD_XVISOR_BASIC_FIRMWARE_SOURCE_PATH} O=${BUILD_XVISOR_OUTPUT_PATH} -j ${BUILD_NUM_THREADS}

echo "=== Create Xvisor Disk Image for Basic Guest ==="
if [ ! -d ${BUILD_XVISOR_DISK_BASIC_PATH} ]; then
	mkdir -p ${BUILD_XVISOR_DISK_BASIC_PATH}
	mkdir -p ${BUILD_XVISOR_DISK_BASIC_PATH}/tmp
	mkdir -p ${BUILD_XVISOR_DISK_BASIC_PATH}/system
	cp -f ${BUILD_XVISOR_SOURCE_PATH}/docs/banner/roman.txt ${BUILD_XVISOR_DISK_BASIC_PATH}/system/banner.txt
	cp -f ${BUILD_XVISOR_SOURCE_PATH}/docs/logo/xvisor_logo_name.ppm ${BUILD_XVISOR_DISK_BASIC_PATH}/system/logo.ppm
	mkdir -p ${BUILD_XVISOR_DISK_BASIC_PATH}/images/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}
	dtc -q -I dts -O dtb -o ${BUILD_XVISOR_DISK_BASIC_PATH}/images/${BUILD_XVISOR_TESTS_DIR}/${BUILD_XVISOR_GUEST_DTS_BASENAME}.dtb ${BUILD_XVISOR_GUEST_DTS_PATH}
	cp -f ${BUILD_XVISOR_OUTPUT_PATH}/tests/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/basic/firmware.bin ${BUILD_XVISOR_DISK_BASIC_PATH}/images/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/firmware.bin
	cp -f ${BUILD_XVISOR_SOURCE_PATH}/tests/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/basic/nor_flash.list ${BUILD_XVISOR_DISK_BASIC_PATH}/images/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/nor_flash.list
	cp -f ${BUILD_XVISOR_SOURCE_PATH}/tests/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/xscript/${BUILD_GUEST_XSCRIPT}.xscript ${BUILD_XVISOR_DISK_BASIC_PATH}/boot.xscript
fi
if [ ! -f ${BUILD_XVISOR_DISK_BASIC_EXT2_PATH} ]; then
	genext2fs -B 1024 -b 32768 -d ${BUILD_XVISOR_DISK_BASIC_PATH} ${BUILD_XVISOR_DISK_BASIC_EXT2_PATH}
fi

if [ "${BUILD_XVISOR_ONLY}" == "yes" ]; then
	export ARCH=${BUILD_XVISOR_ARCH}
	export CROSS_COMPILE=${BUILD_XVISOR_CROSS_COMPILE}
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
	cp -f ${BUILD_LINUX_SOURCE_PATH}/arch/${BUILD_LINUX_ARCH}/configs/${BUILD_LINUX_DEFCONFIG} ${BUILD_LINUX_SOURCE_PATH}/arch/${BUILD_LINUX_ARCH}/configs/tmp-${BUILD_GUEST_TYPE}_defconfig
	${BUILD_XVISOR_SOURCE_PATH}/tests/common/scripts/update-linux-defconfig.sh -p ${BUILD_LINUX_SOURCE_PATH}/arch/${BUILD_LINUX_ARCH}/configs/tmp-${BUILD_GUEST_TYPE}_defconfig -f ${BUILD_LINUX_DEFCONFIG_EXTRA}
	make ARCH=${BUILD_LINUX_ARCH} -C ${BUILD_LINUX_SOURCE_PATH} O=${BUILD_LINUX_OUTPUT_PATH} tmp-${BUILD_GUEST_TYPE}_defconfig
fi
if [ ! -f ${BUILD_LINUX_OUTPUT_PATH}/vmlinux ]; then
	make ARCH=${BUILD_LINUX_ARCH} -C ${BUILD_LINUX_SOURCE_PATH} O=${BUILD_LINUX_OUTPUT_PATH} -j ${BUILD_NUM_THREADS} Image dtbs
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

echo "=== Create Xvisor Disk Image for Linux Guest ==="
if [ ! -d ${BUILD_XVISOR_DISK_LINUX_PATH} ]; then
	mkdir -p ${BUILD_XVISOR_DISK_LINUX_PATH}
	mkdir -p ${BUILD_XVISOR_DISK_LINUX_PATH}/tmp
	mkdir -p ${BUILD_XVISOR_DISK_LINUX_PATH}/system
	cp -f ${BUILD_XVISOR_SOURCE_PATH}/docs/banner/roman.txt ${BUILD_XVISOR_DISK_LINUX_PATH}/system/banner.txt
	cp -f ${BUILD_XVISOR_SOURCE_PATH}/docs/logo/xvisor_logo_name.ppm ${BUILD_XVISOR_DISK_LINUX_PATH}/system/logo.ppm
	mkdir -p ${BUILD_XVISOR_DISK_LINUX_PATH}/images/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}
	dtc -q -I dts -O dtb -o ${BUILD_XVISOR_DISK_LINUX_PATH}/images/${BUILD_XVISOR_TESTS_DIR}/${BUILD_XVISOR_GUEST_DTS_BASENAME}.dtb ${BUILD_XVISOR_GUEST_DTS_PATH}
	cp -f ${BUILD_XVISOR_OUTPUT_PATH}/tests/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/basic/firmware.bin ${BUILD_XVISOR_DISK_LINUX_PATH}/images/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/firmware.bin
	cp -f ${BUILD_XVISOR_SOURCE_PATH}/tests/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/linux/nor_flash.list ${BUILD_XVISOR_DISK_LINUX_PATH}/images/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/nor_flash.list
	cp -f ${BUILD_XVISOR_SOURCE_PATH}/tests/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/linux/cmdlist ${BUILD_XVISOR_DISK_LINUX_PATH}/images/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/cmdlist
	cp -f ${BUILD_XVISOR_SOURCE_PATH}/tests/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/xscript/${BUILD_GUEST_XSCRIPT}.xscript ${BUILD_XVISOR_DISK_LINUX_PATH}/boot.xscript
	cp -f ${BUILD_LINUX_OUTPUT_PATH}/arch/${BUILD_LINUX_ARCH}/boot/Image ${BUILD_XVISOR_DISK_LINUX_PATH}/images/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/Image
	if [ ! -z "${BUILD_XVISOR_LINUX_DTS_PATH}" ]; then
		dtc -q -I dts -O dtb -o ${BUILD_XVISOR_DISK_LINUX_PATH}/images/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/${BUILD_LINUX_DTB_NAME} ${BUILD_XVISOR_LINUX_DTS_PATH}
	else
		cp -f ${BUILD_LINUX_OUTPUT_PATH}/arch/${BUILD_LINUX_ARCH}/boot/dts/${BUILD_LINUX_DTB_NAME} ${BUILD_XVISOR_DISK_LINUX_PATH}/images/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/${BUILD_LINUX_DTB_NAME}
	fi
	cp -f ${BUILD_BUSYBOX_ROOTFS_CPIO_PATH} ${BUILD_XVISOR_DISK_LINUX_PATH}/images/${BUILD_XVISOR_TESTS_DIR}/${BUILD_GUEST_TYPE}/rootfs.img
fi
if [ ! -f ${BUILD_XVISOR_DISK_LINUX_EXT2_PATH} ]; then
	genext2fs -B 1024 -b 32768 -d ${BUILD_XVISOR_DISK_LINUX_PATH} ${BUILD_XVISOR_DISK_LINUX_EXT2_PATH}
fi

echo "=== Install Images ==="
mkdir -p ${BUILD_XVISOR_INSTALL_PATH}
make -C ${BUILD_XVISOR_SOURCE_PATH} O=${BUILD_XVISOR_OUTPUT_PATH} I=${BUILD_XVISOR_INSTALL_PATH} ARCH=${BUILD_XVISOR_ARCH} install
mkdir -p ${BUILD_GUEST_BASIC_INSTALL_PATH}
make -C ${BUILD_XVISOR_BASIC_FIRMWARE_SOURCE_PATH} O=${BUILD_XVISOR_OUTPUT_PATH} I=${BUILD_GUEST_BASIC_INSTALL_PATH} ARCH=${BUILD_XVISOR_ARCH} install
mkdir -p ${BUILD_GUEST_LINUX_INSTALL_PATH}
cp -f ${BUILD_LINUX_OUTPUT_PATH}/System.map ${BUILD_GUEST_LINUX_INSTALL_PATH}
cp -f ${BUILD_LINUX_OUTPUT_PATH}/vmlinux ${BUILD_GUEST_LINUX_INSTALL_PATH}
cp -f ${BUILD_LINUX_OUTPUT_PATH}/arch/${BUILD_LINUX_ARCH}/boot/Image ${BUILD_GUEST_LINUX_INSTALL_PATH}
cp -f ${BUILD_XVISOR_DISK_BASIC_EXT2_PATH} ${BUILD_GUEST_INSTALL_PATH}
cp -f ${BUILD_XVISOR_DISK_LINUX_EXT2_PATH} ${BUILD_GUEST_INSTALL_PATH}

export ARCH=${BUILD_XVISOR_ARCH}
export CROSS_COMPILE=${BUILD_XVISOR_CROSS_COMPILE}
