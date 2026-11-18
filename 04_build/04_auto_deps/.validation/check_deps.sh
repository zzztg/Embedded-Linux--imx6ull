#!/bin/sh
set -eu
cd /mnt/c/Users/dingf/Desktop/imx6ull/04_build/04_auto_deps
mkdir -p .validation/project
cp main.c io_utils.c io_utils.h app_message.h Makefile .validation/project/
make --no-print-directory -C .validation/project
cat .validation/project/main.d .validation/project/io_utils.d
file .validation/project/file_app
sleep 1
touch .validation/project/app_message.h
printf 'Private message header changed; plan and build:\n'
make --no-print-directory -C .validation/project -n
make --no-print-directory -C .validation/project
sleep 1
touch .validation/project/io_utils.h
printf 'Shared interface header changed; plan and build:\n'
make --no-print-directory -C .validation/project -n
make --no-print-directory -C .validation/project
