cmake_minimum_required(VERSION 3.9)
project(fuzzwarp)

add_definitions(-DTIMEWARP_MODE)


add_definitions(-DPREFIX=".")
add_definitions(-DHELPER_PATH="./lib/afl")
add_definitions(-DBIN_PATH="./bin")
add_definitions(-DVERSION="fuzzwarp")

add_definitions(-DAFL_PATH="./lib/afl")
add_definitions(-DHELPER_PATH="./lib/afl")
add_definitions(-DDOC_PATH="./share/doc/afl")
add_definitions(-DMISC_PATH="./share/afl")

# PROGS intentionally omit afl-as, which gets installed elsewhere.


set(CMAKE_CXX_STANDARD 11)

add_executable(fuzzwarp
        experimental/post_library/post_library.so.c
        experimental/post_library/post_library_png.so.c
        fuzzwarp/timewarp_dev/src/afl-timewarp.c
        fuzzwarp/timewarp_dev/src/timewarp_test.c
        fuzzwarp/reviveme.c
        libdislocator/libdislocator.so.c
        libtokencap/libtokencap.so.c
        qemu_mode/patches/afl-qemu-cpu-inl.h
        afl-analyze.c
        afl-as.c
        afl-as.h
        afl-fuzz.c
        afl-gcc.c
        afl-gotcpu.c
        afl-showmap.c
        afl-tmin.c
        alloc-inl.h
        config.h
        debug.h
        hash.h
        test-instr.c
        types.h fuzzwarp/timewarp_dev/src/afl-timewarp.h)
