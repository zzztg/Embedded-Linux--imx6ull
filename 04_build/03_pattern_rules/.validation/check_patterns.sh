#!/bin/sh
set -eu
cd /mnt/c/Users/dingf/Desktop/imx6ull/04_build/03_pattern_rules
mkdir -p .validation/project
cp main.c io_utils.c io_utils.h Makefile .validation/project/
printf 'Fresh build plan:\n'
make --no-print-directory -C .validation/project -n
printf 'Executing the fresh build:\n'
make --no-print-directory -C .validation/project
file .validation/project/file_app
sleep 1
touch .validation/project/io_utils.c
printf 'One source changed; plan then actual build:\n'
make --no-print-directory -C .validation/project -n
make --no-print-directory -C .validation/project
sleep 1
touch .validation/project/io_utils.h
printf 'Shared header changed; build plan:\n'
make --no-print-directory -C .validation/project -n
