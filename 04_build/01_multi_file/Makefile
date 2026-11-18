# 第一版使用明确的文件名和命令，下一节再学习变量与通用规则。
file_app: main.o io_utils.o
	arm-linux-gnueabihf-gcc -static main.o io_utils.o -o file_app

main.o: main.c io_utils.h
	arm-linux-gnueabihf-gcc -std=c11 -Wall -Wextra -c main.c -o main.o

io_utils.o: io_utils.c io_utils.h
	arm-linux-gnueabihf-gcc -std=c11 -Wall -Wextra -c io_utils.c -o io_utils.o

.PHONY: clean
clean:
	rm -f main.o io_utils.o file_app
