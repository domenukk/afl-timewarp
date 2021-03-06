#!/usr/bin/env sh

cd ../build
export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1
export AFL_SKIP_CPUFREQ=1
export AFL_PATH=$PWD/..
export AFL_PRELOAD=../libdislocator/libdislocator.so

rm -rf ../fuzzwarp/test_out_tmp

./fuzzwarp -i ../fuzzwarp/test_in -o ../fuzzwarp/test_out_tmp -m none -t 1000 -Q -W -- "./reviveme" run

