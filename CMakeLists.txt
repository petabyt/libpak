cmake_minimum_required(VERSION 3.10)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
project(pak)

option(PAK_INCLUDE_TEST "cli program" OFF)
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
		src/bluez.c
		src/nm_wifi.c
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
	add_executable(test
		src/test.c
		src/quickjs_wifi.c
	)
	target_link_libraries(test
		PRIVATE
		${QUICKJS_LIB}
		pak
		m
	)
endif()
