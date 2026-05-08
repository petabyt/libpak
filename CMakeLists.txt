cmake_minimum_required(VERSION 3.10)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
project(pak)

set(CMAKE_C_FLAGS_DEBUG -g)
set(CMAKE_CXX_FLAGS_DEBUG -g)
set(CMAKE_C_FLAGS_RELWITHDEBINFO -g)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO -g)

option(PAK_INCLUDE_CLI "CLI program" ON)
option(PAK_INCLUDE_WIFI "WiFi APIs" ON)
option(PAK_INCLUDE_BT "Bluetooth APIs" ON)
option(PAK_INCLUDE_RT "Runtime libraries" ON)
option(PAK_INCLUDE_EXIF "EXIF Parser" ON)
option(PAK_QUICKJS "QuickJS bindings" ON)
option(PAK_WASM "WAMR bindings" ON)

set(LIBPAK_SOURCES)

if(PAK_QUICKJS)
	list(APPEND LIBPAK_SOURCES src/bindings/quickjs_wifi.c src/bindings/quickjs_socket.c src/bindings/quickjs_runtime.c)
endif()
if(PAK_WASM)
	list(APPEND LIBPAK_SOURCES src/bindings/wasm.c)
endif()
if(PAK_INCLUDE_EXIF)
	list(APPEND LIBPAK_SOURCES src/exif.c)
endif()

if(ANDROID)
	add_library(usb-1.0 STATIC
		src/android/usb.c
	)
	target_include_directories(usb-1.0 PUBLIC ${PROJECT_SOURCE_DIR}/src/android)
	add_library(pak STATIC
		${LIBPAK_SOURCES}
		src/android/bluetooth.c
		src/android/android.c
		src/android/wifi.c
	)
	find_library(log-lib log)
	find_library(android-lib android)
	target_link_libraries(pak PRIVATE
		${log-lib}
		${android-lib}
	)
	if(PAK_QUICKJS AND TARGET qjs)
		target_link_libraries(pak PRIVATE qjs)
	endif()
	if(PAK_WASM AND TARGET vmlib)
		target_link_libraries(pak PRIVATE vmlib)
	endif()
elseif(LINUX)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(DBUS IMPORTED_TARGET GLOBAL dbus-1)
	pkg_check_modules(BLUETOOTH IMPORTED_TARGET GLOBAL bluez)
	add_library(pak STATIC
		${LIBPAK_SOURCES}
		src/linux/dbus.c
		src/linux/bluez.c
		src/linux/networkmanager.c
		src/common.c
	)
	target_link_libraries(pak PRIVATE PkgConfig::DBUS PkgConfig::BLUETOOTH)
	if(PAK_QUICKJS)
		find_library(QUICKJS_LIB
			NAMES quickjs
			PATH_SUFFIXES quickjs
			REQUIRED
		)
		target_include_directories(pak PUBLIC /usr/include/quickjs)
		target_link_libraries(pak PRIVATE ${QUICKJS_LIB} m)
	endif()
	if(PAK_WASM)
		find_library(WAMR_LIB
			NAMES iwasm
			REQUIRED
		)
		target_link_libraries(pak PRIVATE ${WAMR_LIB})
	endif()
endif()

target_include_directories(pak
	PUBLIC
	${PROJECT_SOURCE_DIR}/include
)

if(PAK_INCLUDE_CLI AND NOT ANDROID)
	add_executable(pakit
		src/cli.c
		src/cli_runtime.c
		examples/dummy.c
	)
	target_link_libraries(pakit
		PRIVATE
		pak
		m
	)
	install(TARGETS pakit)

	add_executable(bt examples/bt.c)
	target_link_libraries(bt PRIVATE pak m)
endif()
