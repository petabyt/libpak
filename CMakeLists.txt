cmake_minimum_required(VERSION 3.10)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
project(pak)

set(CMAKE_C_FLAGS_DEBUG -g)
set(CMAKE_CXX_FLAGS_DEBUG -g)
set(CMAKE_C_FLAGS_RELWITHDEBINFO -g)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO -g)

option(PAK_INCLUDE_TEST "cli program" ON)
option(PAK_INCLUDE_WIFI "WiFi APIs" ON)
option(PAK_INCLUDE_BT "Bluetooth APIs" ON)

if(ANDROID)
	add_library(pak SHARED
		src/android_bluetooth.c
	)
elseif(LINUX)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(DBUS IMPORTED_TARGET GLOBAL dbus-1)
	pkg_check_modules(BLUETOOTH IMPORTED_TARGET GLOBAL bluez)
	add_library(pak STATIC 
		src/dbus.c
		src/bluez.c
		src/nm_wifi.c
		src/common.c
	)
	target_link_libraries(pak PRIVATE PkgConfig::DBUS PkgConfig::BLUETOOTH)
endif()

target_include_directories(pak
	PUBLIC
	${PROJECT_SOURCE_DIR}/include
)

if(PAK_INCLUDE_TEST)
	find_library(QUICKJS_LIB
	    NAMES quickjs
	    PATH_SUFFIXES quickjs
	    REQUIRED
	)
	add_executable(pakcli
		src/cli.c
		bindings/quickjs_wifi.c
		bindings/quickjs_socket.c
	)
	target_link_libraries(pakcli
		PRIVATE
		${QUICKJS_LIB}
		pak
		m
	)
	add_executable(veecar examples/veecar.c)
	target_link_libraries(veecar PRIVATE pak)
endif()
