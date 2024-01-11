cmake_minimum_required (VERSION 3.5.1)

project (Oasis)

message (STATUS "CMAKE_TOOL_CHAINFILE='${CMAKE_TOOLCHAIN_FILE}'")
message (STATUS "Build system is \"${CMAKE_SYSTEM_NAME}\".")
message (STATUS "Build type is \"${CMAKE_BUILD_TYPE}\".")
message (STATUS "Compiler ID is \"${CMAKE_CXX_COMPILER_ID}\".")
message (STATUS "Arch is \"${ARCH}\".")
message (STATUS "CPU is \"${TARGET_CPU}\".")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

message (STATUS "CMAKE_MODULE_PATH='${CMAKE_MODULE_PATH}'")
message (STATUS "CMAKE_ROOT=${CMAKE_ROOT}")

enable_language(ASM)

#if (NOT "${ARCH}" STREQUAL "arm")
#	message (FATAL_ERROR "${ARCH} not yet supported!")
#endif()

if (WIN32)
	set(PLATFORM "windows")
elseif (APPLE)
	set(PLATFORM "mac")
elseif (UNIX AND NOT APPLE)
	#==> if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(LINUX TRUE)
	set(PLATFORM "linux")
else ()
	message (FATAL_ERROR "Unknown platform" )
endif()

if (NOT CMAKE_SYSTEM_PROCESSOR)
	if ("${ARCH}" STREQUAL "arm")
		set( CMAKE_SYSTEM_PROCESSOR "arm" )
	else()
		set( CMAKE_SYSTEM_PROCESSOR "i686" )
	endif()
endif()

# debugging
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DDBG)
	set(BUILD_DEBUG TRUE)
elseif (CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")	
	set(BUILD_MINSIZEREL TRUE)
endif()



if ("${ARCH}" STREQUAL "arm")
	if ("${TARGET_CPU}" STREQUAL "i3")
		add_definitions(-DTARGET_CPU_I3 -DSOC_VENDOR_ALLWINNER)
		set(TARGET_SOC_VENDOR "allwinner")
		set(SOC_VENDOR "Aw")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wa,-mimplicit-it=thumb")			
	elseif("${TARGET_CPU}" STREQUAL "v3")
		add_definitions(-DTARGET_CPU_V3 -DSOC_VENDOR_ALLWINNER)
		set(TARGET_SOC_VENDOR "allwinner")
		set(SOC_VENDOR "Aw")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wa,-mimplicit-it=thumb")			
	elseif("${TARGET_CPU}" STREQUAL "v5")
		add_definitions(-DTARGET_CPU_V5 -DSOC_VENDOR_ALLWINNER)
		set(TARGET_SOC_VENDOR "allwinner")
		set(SOC_VENDOR "Aw")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wa,-mimplicit-it=thumb")			
	elseif("${TARGET_CPU}" STREQUAL "v536")
		add_definitions(-DTARGET_CPU_V536 -DSOC_VENDOR_ALLWINNER)
		set(TARGET_SOC_VENDOR "allwinner")
		set(SOC_VENDOR "Aw")
		#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wa,-mimplicit-it=thumb")			
	elseif("${TARGET_CPU}" STREQUAL "imx6ull")
		add_definitions(-DTARGET_CPU_IMX6ULL -DSOC_VENDOR_FREESCALE)
		set(TARGET_SOC_VENDOR "freescale")
		set(SOC_VENDOR "Freescale")
		#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wa,-mimplicit-it=thumb")		
	elseif("${TARGET_CPU}" STREQUAL "rpi3" OR "${TARGET_CPU}" STREQUAL "rpi4")
		add_definitions(-DTARGET_CPU_RPI -DSOC_VENDOR_RASPBERRYPI)
		if("${TARGET_CPU}" STREQUAL "rpi3")
			add_definitions(-DTARGET_CPU_RPI3)
		else("${TARGET_CPU}" STREQUAL "rpi4")
			add_definitions(-DTARGET_CPU_RPI4)
		endif()
		set(TARGET_SOC_VENDOR "raspberrypi")
		set(SOC_VENDOR "RaspberryPi")
		#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wa,-mimplicit-it=thumb")		
	else()
		message(FATAL_ERROR "TARGET_CPU not defined")
	endif()
elseif ("${ARCH}" STREQUAL "x86_64")
	add_definitions(-DTARGET_CPU_X86_64 -DSOC_VENDOR_INTEL)
	set(TARGET_SOC_VENDOR "intel")
	set(SOC_VENDOR "Intel")
	#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wa,-mimplicit-it=thumb")		
else()
	message(FATAL_ERROR "ARCH ${ARCH} not defined")	
endif()

if ("${PRODUCT_WHAT}" STREQUAL "mediahills")
	add_definitions(-DPRODUCT_MEDIAHILLS)
elseif ("${PRODUCT_WHAT}" STREQUAL "datech")
	add_definitions(-DPRODUCT_DATECH)
elseif ("${PRODUCT_WHAT}" STREQUAL "ma")
	add_definitions(-DPRODUCT_MA)
elseif ("${PRODUCT_WHAT}" MATCHES "magic*")
	add_definitions(-DPRODUCT_MAGIC)
elseif ("${PRODUCT_WHAT}" STREQUAL "zero608lv")
	add_definitions(-DPRODUCT_ZERO608LV)
elseif ("${PRODUCT_WHAT}" MATCHES "robotis*")
	add_definitions(-DPRODUCT_ROBOTIS)
	if ("${ARCH}" STREQUAL "x86_64")
		#set(INTEL_MFX ON)
		#add_definitions(-DSUPPORT_INTEL_MFX) #enable intel media sdk
		set(INTEL_MFX OFF)
	endif()
elseif ("${PRODUCT_WHAT}" MATCHES "superbible*")
	add_definitions(-DPRODUCT_SUPERBIBLE)
elseif ("${PRODUCT_WHAT}" MATCHES "thes-photoscan-host")
	add_definitions(-DPRODUCT_THES_PHOTOSCAN_HOST)
elseif ("${PRODUCT_WHAT}" STREQUAL "gwlindenis")
	add_definitions(-DPRODUCT_GWLINDENIS)
elseif ("${PRODUCT_WHAT}" STREQUAL "thes_photoscan")
	add_definitions(-DPRODUCT_THES_PHOTOSCAN)
endif()	


if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
	# using Clang or AppleClang
	#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
	set(CMAKE_COMPILER_IS_CLANG TRUE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -stdlib=libc++")
elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
	# using GCC
	if ( MINGW )
		#__MINGW32__  defined
		#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
		set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -municode" )
		# to use %jd for off_t  and %zu for size_t, %zd for ssize_t
		add_definitions(-D__USE_MINGW_ANSI_STDIO=1)
	else ()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
		#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
	endif ()
elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "Intel")
	# using Intel C++
elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
	# using Visual Studio C++
	set(CMAKE_DEBUG_POSTFIX "d")
	#add_definitions(-D_UNICODE) #see below
endif()

if (WIN32)
	add_definitions(-DWIN32 -D_UNICODE -DUNICODE)
endif ()

# warning: 'template<class> class std::auto_ptr' is deprecated
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-color-diagnostic")

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")

# warning: converting from <pointer member function> to <c function pointer> pointer
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-pmf-conversions")

# to work with tcmalloc
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer")

# ASAN address sanitizer (not work together with tcmalloc???) need -lasan
# see http://stackoverflow.com/questions/42712555/address-santizer-fsanitize-address-works-with-tcmalloc

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")

#-fsanitize=address,signed-integer-overflow,shift
#-D_GLIBCXX_DEBUG

if ("${ARCH}" STREQUAL "arm")
	if("${TARGET_CPU}" STREQUAL "rpi3" OR "${TARGET_CPU}" STREQUAL "rpi4")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a -mfpu=neon-fp-armv8 ")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv7-a -mfpu=neon ")
	endif()
	if (BUILD_DEBUG)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mapcs-frame -mapcs-stack-check")
	endif()

	#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math")
	#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftree-vectorize")

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-strong")

	##set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsingle-precision-constant")
	

endif()

add_definitions(-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64)


#when using gnueabihf toolchain
if ("${ARCH}" STREQUAL "arm")
if (GCC_HARDFP)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfloat-abi=hard")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfloat-abi=hard")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfloat-abi=softfp")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfloat-abi=softfp")
endif()
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")


if (BUILD_DEBUG)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
elseif (BUILD_MINSIZEREL)	
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -DNDEBUG")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os -DNDEBUG")
else()
	#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
	#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -DNDEBUG")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -DNDEBUG")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -DNDEBUG")
endif()

if (NOT "${BUILD_WHAT}" STREQUAL "xpack")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
endif()



#gdb bt only
#if("${TARGET_CPU}" STREQUAL "v536")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--rpath=/data")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--dynamic-linker=/data/libc.so")
#endif()

#################################################################################################################
#################################################################################################################
#################################################################################################################

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++ -ggdb")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc -static-libstdc++ -ggdb")

# do not automatically add '-rdynamic' option
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

#set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/output/${PLATFORM}/lib)
#set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/output/${PLATFORM})
#set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/output/${PLATFORM})
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/output)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/output)
#set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/../target/${PLATFORM}"  CACHE PATH "default installation path" FORCE)
set(CMAKE_INSTALL_PREFIX "${PROJECT_BINARY_DIR}/install"  CACHE PATH "default installation path" FORCE)

message(STATUS "Installation directory prefix is '${CMAKE_INSTALL_PREFIX}'")



include_directories(SYSTEM "${PROJECT_SOURCE_DIR}")
include_directories(SYSTEM "${PROJECT_SOURCE_DIR}/include/${SOC_VENDOR}/${TARGET_CPU}")
include_directories(SYSTEM "${PROJECT_SOURCE_DIR}/include/${SOC_VENDOR}/${TARGET_CPU}/${PRODUCT_WHAT}")
include_directories(SYSTEM "${PROJECT_SOURCE_DIR}/${ARCH}/${SOC_VENDOR}/${TARGET_CPU}/${PRODUCT_WHAT}/include")

include_directories(SYSTEM "${PROJECT_SOURCE_DIR}/include")
include_directories(SYSTEM "${PROJECT_SOURCE_DIR}/lib/tinyxml")

#-Wl,-rpath,path1:path2:path3...
link_directories(${PROJECT_SOURCE_DIR}/${ARCH}/${SOC_VENDOR}/${TARGET_CPU}/${PRODUCT_WHAT}/lib)
link_directories(${PROJECT_SOURCE_DIR}/lib/tinyxml)

#strip
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s")

#add_library(ttyraw STATIC ttyraw.cpp)

set(OASIS_LIBRARIES oasis_rtsp  oasis_net oasis_ui oasis_fs oasis_media oasis_disp oasis_certs oasis_codec oasis_adas oasis_md oasis_util oasis)
if ("${TARGET_CPU}" STREQUAL "v5" OR "${TARGET_CPU}" STREQUAL "v536")
	set(OASIS_LIBRARIES ${OASIS_LIBRARIES} oasis_v5)
endif()


if ("${TARGET_SOC_VENDOR}" STREQUAL "allwinner")

set(I3V3_LIBRARIES tixml evdev rt pthread jsoncpp freetype png16 z cdc_base cdc_vencoder cdc_ve cdc_memory cdc_vdecoder) 
set(V5V536_LIBRARIES tixml evdev rt pthread jsoncpp freetype png16 z cdc_base VE MemAdapter videoengine awh264 awh265 awmjpegplus venc_base venc_codec vencoder vdecoder _ise_mo ISP isp_ini) 

if ("${TARGET_CPU}" STREQUAL "v3" OR "${TARGET_CPU}" STREQUAL "i3")
	set(PLATFORM_LIBRARIES ${I3V3_LIBRARIES})
elseif("${TARGET_CPU}" STREQUAL "v5" OR "${TARGET_CPU}" STREQUAL "v536")
	set(PLATFORM_LIBRARIES ${V5V536_LIBRARIES})
endif()

add_library(ttyraw STATIC ttyraw.cpp)

add_library(SB_System STATIC SB_System.cpp)
add_library(led_process STATIC led_process.cpp)
add_library(gsensor STATIC gsensor.cpp)
add_library(datools STATIC datools.cpp)
add_library(sysfs STATIC sysfs.cpp)
add_library(dagpio STATIC dagpio.cpp)

#recorder3 (new API)
#set(EXECUTABLE_NAME recorder)

#file( GLOB APP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp )
#foreach( SOURCE_PATH ${APP_SOURCES} )
#	get_filename_component(${EXECUTABLE_NAME} ${SOURCE_PATH} NAME_WE)
#    add_executable( ${EXECUTABLE_NAME} ${SOURCE_PATH} )
#    target_link_libraries( ${EXECUTABLE_NAME} ${OASIS_LIBRARIES} ${PLATFORM_LIBRARIES} )
#endforeach( SOURCE_PATH ${APP_SOURCES} )

add_executable(recorder recorder.cpp ConfigTextFile.cpp user_data.cpp hdlc.cpp pai_r_datasaver.cpp evdev_input.cpp data_log.cpp safe_driving_monitoring.cpp bb_micom.cpp system_log.cpp pai_r_data.cpp bb_api.cpp recorder_ex.cpp serial.cpp DaUpStreamDelegate.cpp daSystemSetup.cpp mixwav.cpp wavplay.cpp)
target_link_libraries(recorder ttyraw SB_System led_process dagpio sysfs datools gsensor ${OASIS_LIBRARIES} ${PLATFORM_LIBRARIES})

endif()
