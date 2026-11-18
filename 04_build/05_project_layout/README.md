# 实验 04-05：工程目录与构建顺序

状态：学习者已亲手完成首次构建（退出 0）、ARM 静态程序检查、两个 .d 文件检查，以及再次 make 无需构建。更新 build 目录时间戳后无需构建；更新 include/app_message.h 后，预览与实际均只重新编译 main.c 并链接，退出 0。等待本节上板验证，理解程度不以执行成功判定。

教师在 .validation/project 中隔离验证通过：首次创建目录并生成 ARM 静态程序；.d 路径正确；更新 build 目录时间戳不触发构建；更新 include/app_message.h 只重编译 main.c 并链接。此验证不替代学习者在本节根目录亲手编译，尚未进行本节板端验证。

## 本节目的

继续使用上一节的写文件程序，C 代码保持一致。只改变文件位置，让源码、头文件和生成文件分开放置。

~~~text
05_project_layout/
├── Makefile
├── README.md
├── src/
│   ├── main.c
│   └── io_utils.c
├── include/
│   ├── io_utils.h
│   └── app_message.h
└── build/                 ← 执行 make 时自动创建
    ├── main.o
    ├── io_utils.o
    ├── main.d
    ├── io_utils.d
    └── file_app
~~~

目录名是工程约定，不是 Linux 强制要求。build 中的文件可以由源码重新生成；源码与头文件应保留。

## 承接自动依赖

- 编译器通过 -MMD -MP 生成 .d，记录本次编译实际使用的用户头文件。
- make 通过 -include 读入 .d，结合模式规则判断何时重新编译。
- 只有生成却不读入 .d，在当前规则中，单独更新头文件不会自动触发相应 .o 的重建。
- .d 不参与链接。链接器接收 .o，将目标文件组合成程序。

本节用 -o build/main.o 指定目标文件，GCC 同时把依赖写入 build/main.d，规则目标也为 build/main.o。

## 头文件搜索路径

~~~make
CPPFLAGS := -Iinclude
~~~

CPPFLAGS 是传给预处理器的选项，CPP 在此不是 C++。-I 后跟要加入的头文件搜索目录。

在工程根目录运行 make 时，-Iinclude 指向工程的 include 目录。src/main.c 的 #include "io_utils.h" 首先检查源文件所在目录，随后还会检查 -I 等搜索路径，因此无须把源码改写成 ../include/io_utils.h。

-I 指定搜索位置；-MMD 记录实际依赖；-include 把依赖交给 make。这是三件不同的事。

## 输出路径与模式规则

~~~make
build/%.o: src/%.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@
~~~

构建 build/main.o 时，% 匹配 main，普通依赖为 src/main.c，顺序依赖为 build。

- $< 展开为 src/main.c。
- $@ 展开为 build/main.o。
- 配方必须实际生成它声明的目标。

## 为什么目录在竖线右边

~~~make
build:
	mkdir -p $@
~~~

编译器不会自动创建 -o 指定路径的父目录，所以要先保证 build 存在。

普通依赖要求先更新依赖，而且依赖比目标新时可能触发重建。顺序依赖同样要求先完成依赖，但不把它的时间戳作为重建原因。竖线是 make 语法，不是 Shell 管道。

目录内新增、删除或重命名条目通常会更新目录时间戳。如果把 build 当作普通依赖，目录变化可能引起不必要的重编译。放在竖线右侧表达的是：先准备好目录，但目录时间戳变化本身不要求重编译。

本节 build 是真实目录目标，不标记 .PHONY；all 和 clean 是动作名称，标记 .PHONY。

## WSL：亲手预览与编译

~~~sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/04_build/05_project_layout
cat Makefile
make -n
make
echo $?
file build/file_app
cat build/main.d
cat build/io_utils.d
make
~~~

首次预览应有创建目录、两次编译、一次链接。make -n 只预览，本例不会创建目录。最后再次 make 应无需构建。

## WSL：目录时间戳实验

~~~sh
sleep 1
touch build
make -n
~~~

预期无需编译或链接。这里只更新时间戳，不修改目录内程序。

## WSL：头文件变化仍会正确传播

~~~sh
sleep 1
touch include/app_message.h
make -n
make
echo $?
~~~

预期只重新编译 src/main.c，再链接。读取 build/main.d 可以看到带 include/ 前缀的头文件依赖。

## 上板验证（完成 WSL 编译之后）

先在 COM12 创建目标目录：

~~~sh
mkdir -p /root/linux-study/04_build/05_project_layout
~~~

再回 WSL，保持本节工作目录，传输最终程序：

~~~sh
scp -O build/file_app root@192.168.10.2:/root/linux-study/04_build/05_project_layout/
~~~

最后在 COM12 执行：

~~~sh
cd /root/linux-study/04_build/05_project_layout
chmod +x file_app
./file_app app_note.txt
echo $?
cat app_note.txt
~~~

预期写入 27 字节、退出 0，文件内容为 Hello from Linux file I/O! 加换行。编译产物分目录不会改变程序的运行行为。

## 清理与本节边界

make clean 只移除列出的 .o、.d 和程序，保留 build 目录及源文件。清理后重新 make 会完整编译，不一定再打印 mkdir，因为目录仍存在。

当前规则处理 src 下的一层源文件；新增模块时仍需更新 OBJECTS。编译选项变化仍不会自动触发重建，需要清理后构建。这些限制没有因为自动依赖而消失。

参考：[GNU make 顺序依赖](https://www.gnu.org/software/make/manual/html_node/Prerequisite-Types.html)、[GCC 预处理与依赖选项](https://gcc.gnu.org/onlinedocs/gcc/Preprocessor-Options.html)。
