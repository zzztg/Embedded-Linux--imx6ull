#!/bin/sh
set -eu
cd /mnt/c/Users/dingf/Desktop/imx6ull/04_build/02_make_variables
mkdir -p .validation/project
cp main.c io_utils.c io_utils.h Makefile .validation/project/
make --no-print-directory -C .validation/project
file .validation/project/file_app
make --no-print-directory -C .validation/project -q file_app CFLAGS='-std=c11 -Wall -Wextra -O2'
printf 'Changing CFLAGS alone leaves this existing target up to date.\n'
make --no-print-directory -C .validation/project -n -B CFLAGS='-std=c11 -Wall -Wextra -O2'
