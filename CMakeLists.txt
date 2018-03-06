cmake_minimum_required(VERSION 3.9)
project(fuzzwarp)

add_definitions(-DTIMEWARP_MODE)

set(CMAKE_CXX_STANDARD 11)

add_executable(fuzzwarp
        experimental/argv_fuzzing/argv-fuzz-inl.h
        experimental/persistent_demo/persistent_demo.c
        experimental/post_library/post_library.so.c
        experimental/post_library/post_library_png.so.c
        fuzzwarp/gunnar/shared/plaintext_server/demo.c
        fuzzwarp/gunnar/shared/plaintext_server/print_flag.c
        fuzzwarp/gunnar/shared/plaintext_server/server.c
        fuzzwarp/gunnar/shared/plaintext_server/tools.c
        fuzzwarp/gunnar/shared/plaintext_server/tools.h
        fuzzwarp/gunnar/shared/plaintext_server/wrapper.c
        fuzzwarp/timewarp_dev/src/afl-timewarp.c
        fuzzwarp/timewarp_dev/src/timewarp_test.c
        fuzzwarp/reviveme.c
        libdislocator/libdislocator.so.c
        libtokencap/libtokencap.so.c
        llvm_mode/afl-clang-fast.c
        llvm_mode/afl-llvm-pass.so.cc
        llvm_mode/afl-llvm-rt.o.c
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
