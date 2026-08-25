set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR wasm32)
set(CMAKE_SYSROOT /usr/share/wasi-sysroot)
set(WASI_TARGET_FLAGS "--target=wasm32-wasip1")
set(WASI_SHIM "-include ${CMAKE_CURRENT_LIST_DIR}/include/shim_sockets.h")

set(CMAKE_C_FLAGS                  "${WASI_TARGET_FLAGS} -D_WASI_EMULATED_GETPID ${WASI_SHIM}" CACHE INTERNAL "")
set(CMAKE_C_COMPILER_TARGET        "wasm32-wasip1")
set(CMAKE_C_COMPILER               "/usr/bin/clang")

set(CMAKE_CXX_FLAGS                "${WASI_TARGET_FLAGS} -D_WASI_EMULATED_GETPID ${WASI_SHIM}" CACHE INTERNAL "")
set(CMAKE_CXX_COMPILER_TARGET      "wasm32-wasip1")
set(CMAKE_CXX_COMPILER             "/usr/bin/clang++")

set(CMAKE_EXE_LINKER_FLAGS "-Wl,--export-table,--no-entry,--export=get_module" CACHE INTERNAL "")
set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--export-table,--no-entry" CACHE INTERNAL "")

set(CMAKE_LINKER  "/usr/bin/wasm-ld"                     CACHE INTERNAL "")
set(CMAKE_AR      "/usr/bin/llvm-ar"                     CACHE INTERNAL "")
set(CMAKE_NM      "/usr/bin/llvm-nm"                     CACHE INTERNAL "")
set(CMAKE_OBJDUMP "/usr/bin/llvm-dwarfdump"              CACHE INTERNAL "")
set(CMAKE_RANLIB  "/usr/bin/llvm-ranlib"                 CACHE INTERNAL "")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS},--allow-undefined-file=${CMAKE_CURRENT_LIST_DIR}/symbols.txt" CACHE INTERNAL "")
set(PKG_CONFIG_EXECUTABLE "/bin/false")

set(CMAKE_C_COMPILER_WORKS 1 CACHE INTERNAL "")
set(CMAKE_CXX_COMPILER_WORKS 1 CACHE INTERNAL "")
