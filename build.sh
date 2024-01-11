#!/bin/bash

# exit immediate on error
set -e

export GCC_COLORS='error=01;31:warning=01;35:note=01;36:caret=01;32:locus=01:quote=01'


TOP=`pwd`

ARCH="arm"
TARGET_CPU="v536"
PRODUCT_WHAT="datech"
SOC_VENDOR="Aw"
CMAKE_TOOLCHAIN_FILE="./arm-lindenis-v536-musl.cmake"

TARGET_STRIP_EXEC=
TOOLCHAIN_OPT=
GENERATOR_OPT=
TARGET_OBJCOPY_EXEC=

JLEVELS=1
cpu_cores=`cat /proc/cpuinfo | grep "processor" | wc -l`
if [ ${cpu_cores} -lt 8 ] ; then
	JLEVELS=${cpu_cores}
else
	JLEVELS=`expr ${cpu_cores} / 2`
fi

BUILD_CMD="make -j$JLEVELS"
COMPILER_ID="GNU"

OS_NAME=`uname -s`
echo "OS is $OS_NAME, ${OS_NAME:0:7}"

if [ "${OS_NAME:0:10}" == "MINGW64_NT" ]; then
	#MINGW64_NT-6.1-7601 for MINGW64 gcc
	GENERATOR_OPT=(-G "MSYS Makefiles")  
	COMPILER_ID="MINGW"
elif [ "${OS_NAME:0:7}" == "MSYS_NT" ]; then
	#MSYS_NT-6.1-7601 for MSVC
	GENERATOR_OPT=(-G "Ninja")
	BUILD_CMD="ninja -j$JLEVELS"
	COMPILER_ID="MSVC"
else
	GENERATOR_OPT=()
fi


CMAKE_TOOLCHAIN_FILE="`pwd`/$CMAKE_TOOLCHAIN_FILE"
TARGET_STRIP_EXEC=`grep 'CMAKE_C_COMPILER' $CMAKE_TOOLCHAIN_FILE | awk '{print $2}' | sed -n 's/\(.*\)-gcc)$/\1-strip/p'`
#TARGET_STRIP_EXEC=$(echo $TARGET_STRIP_EXEC | sed 's/$ENV{USER}/$USER/g')
TARGET_STRIP_EXEC="${TARGET_STRIP_EXEC/\$ENV\{USER\}/$USER}"
TARGET_OBJCOPY_EXEC=`grep 'CMAKE_C_COMPILER' $CMAKE_TOOLCHAIN_FILE | awk '{print $2}' | sed -n 's/\(.*\)-gcc)$/\1-objcopy/p'`
#TARGET_OBJCOPY_EXEC=$(echo $TARGET_OBJCOPY_EXEC | sed 's/$ENV{USER}/$USER/g')
TARGET_OBJCOPY_EXEC="${TARGET_OBJCOPY_EXEC/\$ENV\{USER\}/$USER}"
TOOLCHAIN_OPT="-DCMAKE_TOOLCHAIN_FILE=$CMAKE_TOOLCHAIN_FILE"

echo "info: TARGET_STRIP_EXEC=$TARGET_STRIP_EXEC"
echo "info: TARGET_OBJCOPY_EXEC=$TARGET_OBJCOPY_EXEC"
echo "info: TOOLCHAIN_OPT=$TOOLCHAIN_OPT"
echo "info: GENERATOR_OPT=" "${GENERATOR_OPT[@]}"


STRIPCMD="$TARGET_STRIP_EXEC --strip-debug --strip-unneeded "


if [ "$1" == "Debug" ]; then
	BUILD_TYPE=$1
	EXAMPLE_DIRECTORY=./build/Debug
elif [ "$1" == "Release" ]; then
	BUILD_TYPE=$1
	EXAMPLE_DIRECTORY=./build/Release
elif [ "$1" == "MinSizeRel" ]; then
	BUILD_TYPE=$1
	EXAMPLE_DIRECTORY=./build/MinSizeRel
else
	exit
fi


#check cmake path
# if [ -f /usr/local/bin/cmake ]; then
# 	#Darwin or Linux (manually installed)
# 	CMAKE_PATH=/usr/local/bin/cmake
# elif [ -f /usr/bin/cmake ]; then
# 	#Linux
# 	CMAKE_PATH=/usr/bin/cmake
# else
# 	CMAKE_PATH=cmake
# fi

# CMAKE_PATH=cmake

xIFS="$IFS"
IFS=':'
for P in $PATH; do
	#echo $P
	if [ -f $P/cmake ]; then
		CMAKE_PATH="$P/cmake"
		break
	fi
done
IFS="$xIFS"

echo "cmake path is $CMAKE_PATH"
if ! [ -f "$CMAKE_PATH" ]; then
	echo "cmake not found"
	exit
fi

if [ ! -d $EXAMPLE_DIRECTORY ]; then
	mkdir -p $EXAMPLE_DIRECTORY
	cd $EXAMPLE_DIRECTORY
	$CMAKE_PATH "${GENERATOR_OPT[@]}" $TOOLCHAIN_OPT -DCMAKE_BUILD_TYPE=$BUILD_TYPE $TOP -DARCH=$ARCH -DTARGET_CPU=$TARGET_CPU -DPRODUCT_WHAT=$PRODUCT_WHAT
	cd -
	[ $? -ne 0 ] && return 1 
fi

cd $EXAMPLE_DIRECTORY
make VERBOSE=1 -j$JLEVELS
#ninja -j$JLEVELS
cd -

[ $? -ne 0 ] && return 1 

echo -e "\033[33minfo: building Oasis examples finished.\033[0m"

cp ./build/Release/output/recorder ../buildroot-oasis/board/datech/da300_v536_spinor/rootfs_overlay/datech/app
cp ./ini/recorder3-datech-3ch.ini ../buildroot-oasis/board/datech/da300_v536_spinor/userfs
date > ../buildroot-oasis/board/datech/da300_v536_spinor/userfs/reboot.txt

