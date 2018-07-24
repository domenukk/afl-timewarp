cmake_minimum_required(VERSION 3.9)
project(fuzzwarp)

add_definitions(-DTIMEWARP_MODE)

add_definitions(-DPREFIX=".")
add_definitions(-DHELPER_PATH="./lib/afl")
add_definitions(-DBIN_PATH="./bin")

add_definitions(-DAFL_PATH="./lib/afl")
add_definitions(-DHELPER_PATH="./lib/afl")
add_definitions(-DDOC_PATH="./share/doc/afl")
add_definitions(-DMISC_PATH="./share/afl")

# PROGS intentionally omit afl-as, which gets installed elsewhere.


set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)

add_executable(fuzzwarp
        qemu_mode/afl-qemu-cpu-inl.h
        afl-fuzz.c
        alloc-inl.h
        config.h
        debug.h
        hash.h
        types.h
        fuzzwarp/afl-timewarp.h
        fuzzwarp/afl-timewarp.c
        )

add_executable(reviveme
        fuzzwarp/reviveme.c)

add_custom_target(fuzzwarp_example
        DEPENDS fuzzwarp reviveme
        )

add_executable(timewarp_test
        fuzzwarp/timewarp_test.c
        fuzzwarp/afl-timewarp.h
        fuzzwarp/afl-timewarp.c
        )

find_package( ZLIB REQUIRED )
if ( ZLIB_FOUND )
    include_directories( ${ZLIB_INCLUDE_DIRS} )
    target_link_libraries(fuzzwarp ${ZLIB_LIBRARIES} )
endif( ZLIB_FOUND )

target_link_libraries(fuzzwarp ${CMAKE_DL_LIBS})

