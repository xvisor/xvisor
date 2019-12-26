#!/bin/bash

function usage()
{
	echo "Usage:"
	echo " $0 [options]"
	echo "Options:"
	echo "     -h                       Display help or usage (Optional)"
	echo "     -p <xvisor_source_path>  Xvisor source path (Optional)"
	echo "     -o <build_output_path>   Build output path (Optional)"
	echo "     -i <build_install_path>  Build install path (Optional)"
	echo "     -d <tarball_path>        Directory containing Linux and Busybox tarball (Optional)"
	echo "     -j <num_threads>         Number of threads for Make (Optional)"
	echo "     -l <linux_version>       Guest Linux version (Optional)"
	echo "     -b <busybox_version>     Guest Busybox version (Optional)"
	exit 1;
}

# Command line options
BUILD_NUM_THREADS=1
BUILD_OUTPUT_PATH=`pwd`/build
BUILD_INSTALL_PATH=`pwd`/install
BUILD_XVISOR_SOURCE_PATH=`pwd`
BUILD_TARBALL_PATH=`pwd`/tarball
BUILD_LINUX_VERSION="5.4.6"
BUILD_BUSYBOX_VERSION="1.31.1"

while getopts "d:hj:l:i:o:p:" o; do
	case "${o}" in
	b)
		BUILD_BUSYBOX_VERSION=${OPTARG}
		;;
	d)
		BUILD_TARBALL_PATH=${OPTARG}
		;;
	h)
		usage
		;;
	j)
		BUILD_NUM_THREADS=${OPTARG}
		;;
	l)
		BUILD_LINUX_VERSION=${OPTARG}
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
	*)
		usage
		;;
	esac
done
shift $((OPTIND-1))

if [ -z "${BUILD_XVISOR_SOURCE_PATH}" ]; then
	echo "Must specify Xvisor source path"
	usage
fi

if [ ! -d ${BUILD_XVISOR_SOURCE_PATH} ]; then
	echo "Xvisor source path does not exist"
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

BUILD_SCRIPTS_PATH=`dirname $0`


mkdir -p ${BUILD_OUTPUT_PATH}/arm

${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v5 -g versatilepb -s one_guest_versatilepb -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v5 -i ${BUILD_INSTALL_PATH}/arm/v5 -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v5 -g versatilepb -s two_guest_versatilepb -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v5 -i ${BUILD_INSTALL_PATH}/arm/v5 -j ${BUILD_NUM_THREADS}
rm -rf ${BUILD_OUTPUT_PATH}/arm/v5

${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v6 -g realview-eb-mpcore -s one_guest_ebmp -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v6 -i ${BUILD_INSTALL_PATH}/arm/v6 -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v6 -g realview-eb-mpcore -s two_guest_ebmp -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v6 -i ${BUILD_INSTALL_PATH}/arm/v6 -j ${BUILD_NUM_THREADS}
rm -rf ${BUILD_OUTPUT_PATH}/arm/v6

${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v7 -g realview-pb-a8 -s one_guest_pb-a8 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v7 -i ${BUILD_INSTALL_PATH}/arm/v7 -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v7 -g realview-pb-a8 -s two_guest_pb-a8 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v7 -i ${BUILD_INSTALL_PATH}/arm/v7 -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v7 -g vexpress-a9 -s one_guest_vexpress-a9 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v7 -i ${BUILD_INSTALL_PATH}/arm/v7 -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v7 -g vexpress-a9 -s two_guest_vexpress-a9 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v7 -i ${BUILD_INSTALL_PATH}/arm/v7 -j ${BUILD_NUM_THREADS}
rm -rf ${BUILD_OUTPUT_PATH}/arm/v7

${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v7-ve -g realview-pb-a8 -s one_guest_pb-a8 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v7-ve -i ${BUILD_INSTALL_PATH}/arm/v7-ve -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v7-ve -g realview-pb-a8 -s two_guest_pb-a8 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v7-ve -i ${BUILD_INSTALL_PATH}/arm/v7-ve -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v7-ve -g vexpress-a9 -s one_guest_vexpress-a9 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v7-ve -i ${BUILD_INSTALL_PATH}/arm/v7-ve -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v7-ve -g vexpress-a9 -s two_guest_vexpress-a9 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v7-ve -i ${BUILD_INSTALL_PATH}/arm/v7-ve -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v7-ve -g vexpress-a15 -s one_guest_vexpress-a15 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v7-ve -i ${BUILD_INSTALL_PATH}/arm/v7-ve -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v7-ve -g vexpress-a15 -s one_novgic_guest_vexpress-a15 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v7-ve -i ${BUILD_INSTALL_PATH}/arm/v7-ve -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v7-ve -g vexpress-a15 -s two_guest_vexpress-a15 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v7-ve -i ${BUILD_INSTALL_PATH}/arm/v7-ve -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v7-ve -g vexpress-a15 -s two_novgic_guest_vexpress-a15 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v7-ve -i ${BUILD_INSTALL_PATH}/arm/v7-ve -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v7-ve -g vexpress-a15 -s two_pt_guest_vexpress-a15 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v7-ve -i ${BUILD_INSTALL_PATH}/arm/v7-ve -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v7-ve -g virt-v7 -s one_guest_virt-v7 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v7-ve -i ${BUILD_INSTALL_PATH}/arm/v7-ve -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v7-ve -g virt-v7 -s one_novgic_guest_virt-v7 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v7-ve -i ${BUILD_INSTALL_PATH}/arm/v7-ve -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v7-ve -g virt-v7 -s two_guest_virt-v7 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v7-ve -i ${BUILD_INSTALL_PATH}/arm/v7-ve -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v7-ve -g virt-v7 -s two_novgic_guest_virt-v7 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v7-ve -i ${BUILD_INSTALL_PATH}/arm/v7-ve -j ${BUILD_NUM_THREADS}
rm -rf ${BUILD_OUTPUT_PATH}/arm/v7-ve

${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v8 -g realview-pb-a8 -s one_guest_pb-a8 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v8 -i ${BUILD_INSTALL_PATH}/arm/v8 -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v8 -g realview-pb-a8 -s two_guest_pb-a8 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v8 -i ${BUILD_INSTALL_PATH}/arm/v8 -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v8 -g vexpress-a9 -s one_guest_vexpress-a9 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v8 -i ${BUILD_INSTALL_PATH}/arm/v8 -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v8 -g vexpress-a9 -s two_guest_vexpress-a9 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v8 -i ${BUILD_INSTALL_PATH}/arm/v8 -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v8 -g vexpress-a15 -s one_guest_vexpress-a15 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v8 -i ${BUILD_INSTALL_PATH}/arm/v8 -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v8 -g vexpress-a15 -s one_novgic_guest_vexpress-a15 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v8 -i ${BUILD_INSTALL_PATH}/arm/v8 -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v8 -g vexpress-a15 -s two_guest_vexpress-a15 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v8 -i ${BUILD_INSTALL_PATH}/arm/v8 -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v8 -g vexpress-a15 -s two_novgic_guest_vexpress-a15 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v8 -i ${BUILD_INSTALL_PATH}/arm/v8 -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v8 -g vexpress-a15 -s two_pt_guest_vexpress-a15 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v8 -i ${BUILD_INSTALL_PATH}/arm/v8 -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v8 -g virt-v7 -s one_guest_virt-v7 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v8 -i ${BUILD_INSTALL_PATH}/arm/v8 -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v8 -g virt-v7 -s one_novgic_guest_virt-v7 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v8 -i ${BUILD_INSTALL_PATH}/arm/v8 -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v8 -g virt-v7 -s two_guest_virt-v7 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v8 -i ${BUILD_INSTALL_PATH}/arm/v8 -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v8 -g virt-v7 -s two_novgic_guest_virt-v7 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v8 -i ${BUILD_INSTALL_PATH}/arm/v8 -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v8 -g virt-v8 -s one_guest_virt-v8 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v8 -i ${BUILD_INSTALL_PATH}/arm/v8 -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v8 -g virt-v8 -s one_novgic_guest_virt-v8 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v8 -i ${BUILD_INSTALL_PATH}/arm/v8 -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v8 -g virt-v8 -s two_guest_virt-v8 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v8 -i ${BUILD_INSTALL_PATH}/arm/v8 -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-arm-images.sh -a v8 -g virt-v8 -s two_novgic_guest_virt-v8 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/arm/v8 -i ${BUILD_INSTALL_PATH}/arm/v8 -j ${BUILD_NUM_THREADS}
rm -rf ${BUILD_OUTPUT_PATH}/arm/v8

rm -rf ${BUILD_OUTPUT_PATH}/arm


mkdir -p ${BUILD_OUTPUT_PATH}/riscv

${BUILD_SCRIPTS_PATH}/build-riscv-images.sh -a 32b -g virt32 -s one_guest_virt32 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/riscv/rv32 -i ${BUILD_INSTALL_PATH}/riscv/rv32 -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-riscv-images.sh -a 32b -g virt32 -s two_guest_virt32 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/riscv/rv32 -i ${BUILD_INSTALL_PATH}/riscv/rv32 -j ${BUILD_NUM_THREADS}
rm -rf ${BUILD_OUTPUT_PATH}/riscv/rv32

${BUILD_SCRIPTS_PATH}/build-riscv-images.sh -a 64b -g virt32 -s one_guest_virt32 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/riscv/rv64 -i ${BUILD_INSTALL_PATH}/riscv/rv64 -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-riscv-images.sh -a 64b -g virt32 -s two_guest_virt32 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/riscv/rv64 -i ${BUILD_INSTALL_PATH}/riscv/rv64 -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-riscv-images.sh -a 64b -g virt64 -s one_guest_virt64 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/riscv/rv64 -i ${BUILD_INSTALL_PATH}/riscv/rv64 -j ${BUILD_NUM_THREADS}
${BUILD_SCRIPTS_PATH}/build-riscv-images.sh -a 64b -g virt64 -s two_guest_virt64 -d ${BUILD_TARBALL_PATH} -l ${BUILD_LINUX_VERSION} -b ${BUILD_BUSYBOX_VERSION} -p ${BUILD_XVISOR_SOURCE_PATH} -o ${BUILD_OUTPUT_PATH}/riscv/rv64 -i ${BUILD_INSTALL_PATH}/riscv/rv64 -j ${BUILD_NUM_THREADS}
rm -rf ${BUILD_OUTPUT_PATH}/riscv/rv64

rm -rf ${BUILD_OUTPUT_PATH}/riscv


mkdir -p ${BUILD_OUTPUT_PATH}/x86

mkdir -p ${BUILD_OUTPUT_PATH}/x86/x86_64
mkdir -p ${BUILD_INSTALL_PATH}/x86/x86_64
CROSS_COMPILE=
make -C ${BUILD_XVISOR_SOURCE_PATH} ARCH=x86 O=${BUILD_OUTPUT_PATH}/x86/x86_64 I=${BUILD_INSTALL_PATH}/x86/x86_64/xvisor x86_64_generic-defconfig
make -C ${BUILD_XVISOR_SOURCE_PATH} ARCH=x86 O=${BUILD_OUTPUT_PATH}/x86/x86_64 I=${BUILD_INSTALL_PATH}/x86/x86_64/xvisor -j ${BUILD_NUM_THREADS} install
rm -rf ${BUILD_OUTPUT_PATH}/x86/x86_64

rm -rf ${BUILD_OUTPUT_PATH}/x86
