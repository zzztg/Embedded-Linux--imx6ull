#!/bin/sh
set -eu
cd /mnt/c/Users/dingf/Desktop/imx6ull/03_file_io
gcc -std=c11 -Wall -Wextra -Werror 06_buffer/buffer_demo.c -o .validation/buffer_demo_host
arm-linux-gnueabihf-gcc -std=c11 -Wall -Wextra -Werror -static 06_buffer/buffer_demo.c -o .validation/buffer_demo_arm
.validation/buffer_demo_host buffered > .validation/buffered.txt 2> .validation/buffered.log &
buffered_pid=$!
.validation/buffer_demo_host flushed > .validation/flushed.txt 2> .validation/flushed.log &
flushed_pid=$!
trap 'kill "$buffered_pid" "$flushed_pid" 2>/dev/null || :' EXIT
sleep 1
test -s .validation/buffered.log
test -s .validation/flushed.log
test "$(wc -c < .validation/buffered.txt)" -eq 0
test "$(wc -c < .validation/flushed.txt)" -eq 6
printf 'During sleep: buffered=0 bytes, flushed=6 bytes.\n'
wait "$buffered_pid"
wait "$flushed_pid"
trap - EXIT
printf 'HELLO\n' > .validation/buffer_expected.txt
cmp .validation/buffer_expected.txt .validation/buffered.txt
cmp .validation/buffer_expected.txt .validation/flushed.txt
printf 'After normal exit: both files contain the expected 6 bytes.\n'
file .validation/buffer_demo_arm
