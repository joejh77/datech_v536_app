# this one is important
SET(CMAKE_SYSTEM_NAME Linux)
#this one not so much
SET(CMAKE_SYSTEM_VERSION 1)


# specify the cross compiler
SET(EXTERNAL_TOOLCHAIN_ROOT /home/$ENV{USER}/workspace/project/oasis/v536/buildroot-oasis/output/da300_nor/host)
SET(EXTERNAL_TOOLCHAIN_PATH ${EXTERNAL_TOOLCHAIN_ROOT}/bin)
SET(CROSS_COMPILER_PREFIX arm-buildroot-linux-musleabihf-)

#used by build.sh to get the strip command.
SET(CMAKE_C_COMPILER  ${EXTERNAL_TOOLCHAIN_PATH}/arm-buildroot-linux-musleabihf-gcc)
SET(CMAKE_CXX_COMPILER ${EXTERNAL_TOOLCHAIN_PATH}/arm-buildroot-linux-musleabihf-g++)



# where is the target environment 
#SET(CMAKE_FIND_ROOT_PATH /home/cobenhan/workspace/HyperionTech/V3_Linux/lichee/linux-3.4/include)
SET(CMAKE_FIND_ROOT_PATH ${CMAKE_FIND_ROOT_PATH} ${EXTERNAL_TOOLCHAIN_ROOT})
SET(CMAKE_FIND_ROOT_PATH ${CMAKE_FIND_ROOT_PATH} ${EXTERNAL_TOOLCHAIN_ROOT}/arm-buildroot-linux-musleabihf)
SET(CMAKE_FIND_ROOT_PATH ${CMAKE_FIND_ROOT_PATH} ${EXTERNAL_TOOLCHAIN_ROOT}/arm-buildroot-linux-musleabihf/sysroot/usr)
#SET(CMAKE_FIND_ROOT_PATH ${CMAKE_FIND_ROOT_PATH} ${EXTERNAL_TOOLCHAIN_ROOT}/arm-linux-musleabihf/libc/usr)


# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

SET(GCC_HARDFP ON)


