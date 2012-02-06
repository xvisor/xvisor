#! /bin/bash
#/**
# Copyright (C) 2008 yajin (yajin@vm-kernel.org)
# Copyright (c) 2010 Sukanto Ghosh.
# All rights reserved.
#
# Modified the qemu bb_nandflash.sh script to create nandflash image
# using a single flat file and also generate ecc code - Sukanto
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# @file bb_nandflash.sh
# @author Sukanto Ghosh (sukantoghosh@gmail.com)
# @brief script to create beagleboard nand flash image 
# */

if [ ! -r "$1" ]; then
	echo "Usage: $0 <image> <destimage> <path-to-bb_nandflash_ecc-binary>"
	exit -1
fi
if [ 3 -ne "$#" ]; then
	echo "Usage: $0 <image> <destimage> <path-to-bb_nandflash_ecc-binary>"
	exit -1
fi

flash_page_size=2048
flash_oob_size=64
#flash_image_pages=131072

input_image_name=$1
flash_image_name=$2
ecc_binary_path=$3

#beagle board's NAND flash is 2G bit(256M bytes)
if [ ! -e "$2" ]; then
	echo -en "Beagleboard nandflash image \"$flash_image_name\" doesn't exist, creating empty ... "
	echo -en \\0377\\0377\\0377\\0377\\0377\\0377\\0377\\0377 > .8b
	cat .8b .8b > .16b 
	cat .16b .16b >.32b
	cat .32b .32b >.64b  #OOB is 64 bytes
	cat .64b .64b .64b .64b .64b .64b .64b .64b > .512b
	cat .512b .512b .512b .512b .64b>.page  # A page is 2K bytes of data + 64bytes OOB
	cat .page .page .page .page .page .page .page .page >.8page
	cat .8page .8page .8page .8page .8page .8page .8page .8page >.block  # a block = 64 pages
	cat .block .block .block .block .block .block .block .block >.8block
	cat .8block .8block .8block .8block .8block .8block .8block .8block >.64block
	cat .64block .64block .64block .64block .64block .64block .64block .64block >.512block
	cat .512block .512block .512block .512block >$flash_image_name
	rm -rf .8b .16b .32b .64b .512b .page .8page .64sec .block .8block .64block .512block 
	echo "done"
fi

put_no_oob() 
{
	#echo $1
	#echo $2
	image_name=$input_image_name
	image_len=`du -shb $image_name |awk '{print $1}'`
	image_pages=$[$image_len/$flash_page_size]

	if [ 0 -ne $[$image_len%$flash_page_size] ]; then
		image_pages=$[$image_pages+1]
	fi

	#echo $image_len
	#echo $image_pages
	i=0
	while  [ $i -lt $image_pages  ]
  do
  	#echo $i
  	out_offset=$i
  	in_offset=$i
  	#echo "out_offset:"$out_offset
  	#echo "in_offset:"$in_offset
  	dd if=$image_name of=$flash_image_name conv=notrunc count=1 obs=$[$flash_page_size+$flash_oob_size] ibs=$flash_page_size  seek=$out_offset skip=$in_offset >& /dev/null
  	i=$[$i + 1]
	done	
}

echo -en "Loading \"$input_image_name\" into \"$flash_image_name\"  ... "
put_no_oob 
echo "done"

echo -en "Putting ECC data into \"$flash_image_name\" ... "
flash_data_size=$[$image_pages * $flash_page_size]
$ecc_binary_path $flash_image_name 0 $flash_data_size
echo "done"








