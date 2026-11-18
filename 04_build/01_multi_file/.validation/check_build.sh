#!/bin/sh
set -eu
cd /mnt/c/Users/dingf/Desktop/imx6ull/04_build/01_multi_file
mkdir -p .validation/host .validation/arm
gcc -std=c11 -Wall -Wextra -Werror -c main.c -o .validation/host/main.o
gcc -std=c11 -Wall -Wextra -Werror -c io_utils.c -o .validation/host/io_utils.o
gcc .validation/host/main.o .validation/host/io_utils.o -o .validation/host/file_app
.validation/host/file_app .validation/host/app_note.txt
printf 'Hello from Linux file I/O!\n' > .validation/host/expected.txt
cmp .validation/host/expected.txt .validation/host/app_note.txt
arm-linux-gnueabihf-gcc -std=c11 -Wall -Wextra -Werror -c main.c -o .validation/arm/main.o
arm-linux-gnueabihf-gcc -std=c11 -Wall -Wextra -Werror -c io_utils.c -o .validation/arm/io_utils.o
arm-linux-gnueabihf-gcc -static .validation/arm/main.o .validation/arm/io_utils.o -o .validation/arm/file_app
file .validation/arm/main.o .validation/arm/io_utils.o .validation/arm/file_app
