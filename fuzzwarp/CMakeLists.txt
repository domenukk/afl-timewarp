cmake_minimum_required(VERSION 3.9)
project(fuzzwarp)

set(CMAKE_CXX_STANDARD 11)

add_executable(fuzzwarp
        gunnar/shared/plaintext_server/demo.c
        gunnar/shared/plaintext_server/print_flag.c
        gunnar/shared/plaintext_server/server.c
        gunnar/shared/plaintext_server/tools.c
        gunnar/shared/plaintext_server/tools.h
        gunnar/shared/plaintext_server/wrapper.c
        timewarp_dev/src/afl-timewarp.c
        timewarp_dev/src/timewarp_test.c
        timewarp_dev/Makefile
        reviveme.c)
