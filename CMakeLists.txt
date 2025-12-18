cmake_minimum_required(VERSION 3.10)
project(pak)

option(PAK_INCLUDE_TEST "c api test" ON)

if(ANDROID)
	add_library(pak SHARED
		src/android.c
	)
elseif(LINUX)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(DBUS IMPORTED_TARGET GLOBAL dbus-1)
	add_library(pak STATIC 
		src/bluez.c
		src/wifi.c
	)
	target_link_libraries(pak PRIVATE PkgConfig::DBUS)
endif()

target_include_directories(pak
	PUBLIC
	${PROJECT_SOURCE_DIR}/src
)

if(PAK_INCLUDE_TEST)
	add_executable(test
		src/test.c
	)
	target_link_libraries(test
		PRIVATE
		pak
	)
endif()
