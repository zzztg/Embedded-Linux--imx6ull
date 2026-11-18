#!/bin/sh
set -eu
cd /mnt/c/Users/dingf/Desktop/imx6ull/03_file_io
gcc -std=c11 -Wall -Wextra -Werror 04_seek/file_seek.c -o .validation/file_seek_host
arm-linux-gnueabihf-gcc -std=c11 -Wall -Wextra -Werror -static 04_seek/file_seek.c -o .validation/file_seek_arm
printf 'ABCDEFGHIJ\n' > .validation/seek_input.txt
.validation/file_seek_host .validation/seek_input.txt > .validation/seek_trace.log
printf 'ABxyEFGHIJ\n' > .validation/seek_expected.txt
cmp .validation/seek_expected.txt .validation/seek_input.txt
printf 'initial: offset=0\nfirst read: ABCD (4 bytes)\nafter read: offset=4\nmove to byte 2: offset=2\nafter overwrite: offset=4\nfile end: offset=11\nrewind: offset=0\nsecond read: ABxy (4 bytes)\n' > .validation/seek_expected.log
cmp .validation/seek_expected.log .validation/seek_trace.log
status=0
.validation/file_seek_host > .validation/seek_usage.log 2>&1 || status=$?
test "$status" -eq 2
status=0
.validation/file_seek_host .validation/nonexistent_parent_for_seek_test/input.txt > .validation/seek_missing.log 2>&1 || status=$?
test "$status" -eq 1
file .validation/file_seek_arm
printf 'Verified: read position, overwrite without insertion, file size, rewind, and failure paths.\n'
