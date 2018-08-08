#!/usr/bin/env sh
AFL_PRELOAD=../libdislocator/libdislocator.so AFL_SKIP_CPUFREQ=1 AFL_PATH=../ \
strace -tt -yy -y -f -e trace=open,read,write,pipe,socket,dup2,clone,close -s 10000 -o /tmp/strace.log \
./fuzzwarp -i ../fuzzwarp/test_in -o ../fuzzwarp/test_out_tmp -m none -t 1000 -Q -W -- "./reviveme" run

