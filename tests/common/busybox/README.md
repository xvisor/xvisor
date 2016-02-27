# BusyBox RootFS for ARM/ARM64 Guest

## Introduction
[BusyBox](https://busybox.net) provides several stripped-down Unix tools in a
single executable.
It runs in a variety of POSIX environments such as Linux, Android, FreeBSD and
others, such as proprietary kernels, although many of the tools it provides
are designed to work with interfaces provided by the Linux kernel.
It was created for embedded operating systems with very limited resources.
It is released under the terms of the GNU General Public License.
For more info read the
[BusyBox page on Wikipedia](http://en.wikipedia.org/wiki/BusyBox).


## Toolchains
The toolchain to be used for compiling BusyBox rootfs must have C library support.
Currently, we have two options from freely available toolchains:

1. [CodeSourcery Lite ARM GNU/Linux Toolchain](http://www.mentor.com/embedded-software/sourcery-tools/sourcery-codebench/editions/lite-edition/)
    is soft-float toolchain for ARMv5te or higher processors.

    The cross-compile prefix is `arm-none-linux-gnueabi-`.
    It is build with default options: `-mfloat-abi=soft` and `-march=armv5te`.

2. [Linaro ARM GNU/Linux Toolchains](http://releases.linaro.org)
    - **Soft-Float Toolchain** is with software floating point for ARMv7 processors.

        The cross-compile prefix is `arm-linux-gnueabi-`.
        It is build with default options: `-mfloat-abi=soft`,
        `-mfpu-name=vfpv3-d16` and `-march=armv7`.

    - **Hard-Float Toolchain** is with hardware floating point for ARMv7 processors.

        The cross-compile prefix is `arm-linux-gnueabihf-`.
        It is build with default options: `-mfloat-abi=hard`,
        `-mfpu-name=vfpv3-d16` and `-march=armv7`.

Generally it is advisable to build BusyBox with soft-float and armv5te
toolchain so that resulting root filesystem works for ARMv5, ARMv6, and ARMv7
guests.
In real-world scenarios we need a hard-float toolchain for ARMv5 or
higher processors but such toolchain is not freely available and we need to
build this manually using cross-tool scripts.

*You should use Linaro toolchains if you only care about ARMv7 processors.*


## RAMDISK Generation
Please follow the steps below to build a RAMDISK using BusyBox, to be used as
RootFS for ARM Linux guest (replace all `<>` brackets based on your workspace):

1. Setup the build environment for Xvisor based on the selected toolchain

    - ARM64 GNU/Linux Toolchain

        ```bash
        export CROSS_COMPILE=aarch64-linux-gnu-
        ```

    - CodeSourcery Lite ARM GNU/Linux Toolchain

        ```bash
        export CROSS_COMPILE=arm-none-linux-gnueabi-
        ```

    - Linaro ARM GNU/Linux Soft-Float Toolchain

        ```bash
        export CROSS_COMPILE=arm-linux-gnueabi-
        ```

    - Linaro ARM GNU/Linux Hard-Float Toolchain

        ```bash
        export CROSS_COMPILE=arm-linux-gnueabihf-
        ```

2. Go to Xvisor source directory

    ```bash
    cd <xvisor_source_directory>
    ```

3. Copy a `defconfig` file to the Busybox source directory

    ```bash
    cp tests/common/busybox/busybox-<busybox_version>_defconfig <busybox_source_directory>/.config
    ```

4. Go to Busybox source directory

    ```bash
    cd <busybox_source_directory>
    ```

5. Configure Busybox source

    ```bash
    make oldconfig
    ```

6. Build Busybox RootFS under `_install`

    ```bash
    make install
    ```

7. Populate the Busybox RootFS 

    ```bash
    mkdir -p ./_install/etc/init.d
    mkdir -p ./_install/dev
    mkdir -p ./_install/proc
    mkdir -p ./_install/sys
    ln -sf /sbin/init ./_install/init
    cp -f <xvisor_source_directory>/tests/common/busybox/fstab ./_install/etc/fstab
    cp -f <xvisor_source_directory>/tests/common/busybox/rcS ./_install/etc/init.d/rcS
    cp -f <xvisor_source_directory>/tests/common/busybox/motd ./_install/etc/motd
    ```

8. Create a RootFS image using one of the following options (INITRAMFS preferred)

    - INITRAMFS cpio image

        ```bash
        cd ./_install; find ./ | cpio -o -H newc > ../rootfs.img; cd -
        ```

    - OR, INITRAMFS compressed cpio image

        ```bash
        cd ./_install; find ./ | cpio -o -H newc | gzip -9 > ../rootfs.img; cd -
        ```

    - OR, INITRD etx2 image (legacy)

        ```bash
        genext2fs -b 6500 -N 1024 -U -d ./_install ./rootfs.ext2
        ```

