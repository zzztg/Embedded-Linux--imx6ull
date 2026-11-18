#!/bin/sh
set -eu
cd /mnt/c/Users/dingf/Desktop/imx6ull/03_file_io
gcc -std=c11 -Wall -Wextra -Werror 03_read/file_read.c -o .validation/file_read_host
arm-linux-gnueabihf-gcc -std=c11 -Wall -Wextra -Werror -static 03_read/file_read.c -o .validation/file_read_arm

: > .validation/read_empty.bin
printf '0123456789abcdef' > .validation/read_exact.bin
printf 'Hello from Linux file I/O!\n' > .validation/read_text.bin
printf 'A\000B\n' > .validation/read_binary.bin
for input in empty exact text binary; do
    .validation/file_read_host ".validation/read_${input}.bin" > ".validation/read_${input}.out" 2> ".validation/read_${input}.log"
    cmp ".validation/read_${input}.bin" ".validation/read_${input}.out"
done
printf 'read returned 16\nread returned 11\nread returned 0\n' > .validation/read_expected.log
cmp .validation/read_expected.log .validation/read_text.log
status=0
.validation/file_read_host > .validation/read_usage.out 2> .validation/read_usage.log || status=$?
test "$status" -eq 2
status=0
.validation/file_read_host .validation/nonexistent_parent_for_read_test/input.bin > .validation/read_missing.out 2> .validation/read_missing.log || status=$?
test "$status" -eq 1
file .validation/file_read_arm
printf 'Verified: empty file, exact buffer size, 27-byte text, embedded NUL, and two failure paths.\n'
