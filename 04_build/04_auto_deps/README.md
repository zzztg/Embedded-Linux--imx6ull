# 实验 04-04：自动生成头文件依赖

本节继续使用熟悉的写文件程序。main.c 新增包含 app_message.h，消息字符串移到这个头文件的 APP_MESSAGE 宏；io_utils.c 不包含该文件。公共接口 io_utils.h 仍由两个 .c 共同包含。

补充讲解：[从头文件到重新构建](依赖原理详解.md)。学习者已完成下述实验并接受补充讲解，现按要求进入[工程目录与顺序依赖](../05_project_layout/README.md)，在后续实验中继续检查理解。

## 为什么需要自动依赖

上一节 `%.o: %.c io_utils.h` 把公共头文件手写进规则。随着模块包含不同头文件，手工维护既容易遗漏，也可能让不相关模块重编译。本节让编译器根据实际 #include 关系生成依赖。

```make
DEPFLAGS := -MMD -MP
DEPS := $(OBJECTS:.o=.d)

%.o: %.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

-include $(DEPS)
```

- -MMD：正常编译的同时输出用户头文件依赖，通常不包含系统头文件。使用本节的 -o main.o 时生成 main.d。
- -MP：为头文件依赖生成空目标规则，减少删除或更名头文件后旧依赖直接报无规则的情况。这不能让仍然引用缺失头文件的 C 源码编译通过，也不是把头文件声明为总要更新的 .PHONY。
- $(OBJECTS:.o=.d)：把列表中每项的 .o 后缀替换为 .d，本例得到 main.d io_utils.d。
- -include：读入生成的 Makefile 规则；文件尚不存在时允许继续。首次构建还没有 .d，编译后会产生，后续 make 读取它们。

依赖文件包含头文件列表但不提供编译配方；模式规则提供生成 .o 的方法。make 合并这些信息，决定何时运行编译命令。

all 保持在依赖文件读入之前作为默认目标，避免默认构建目标意外变成 .d 文件中的某个 .o。

## 亲手构建与查看（全部在 WSL）

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/04_build/04_auto_deps
make
echo $?
cat main.d
cat io_utils.d
```

预期两次编译、一条链接，状态 0；除 .o 和程序外，还生成两个 .d 文件。

main.d 的主要依赖规则应为：

```make
main.o: main.c io_utils.h app_message.h
```

io_utils.d 的主要规则应为：

```make
io_utils.o: io_utils.c io_utils.h
```

-MP 还会生成 io_utils.h:、app_message.h: 等空规则。这些不是待运行的 Shell 命令。

## 只改一个模块包含的头文件

```sh
sleep 1
touch app_message.h
make -n
make
```

预计只重编译 main.c，再链接，共两条。io_utils.c 没有包含 app_message.h，生成的依赖里也没有它，因此无需重编译。

## 改公共头文件

```sh
sleep 1
touch io_utils.h
make
```

预计两个模块都重编译，再链接，共三条。这里同样没有更改文件内容，只通过时间戳观察依赖判断。

## 调试构建规则时保留的经验

配方指定的输出名应与目标名一致。上一节一度把 main.o 写成 main.ocd，编译命令可以成功生成那个名字，但链接需要的 main.o 并不存在。make 通常依赖配方正确履行目标生成约定，不会替你修正输出路径。make -n 是检查展开后输入输出名的实用工具。

echo $? 紧接要检查的命令。比如先 make、再 ls、最后 echo $?，得到的是 ls 的状态。GNU make 的构建错误状态与被调用编译器的状态也可能不同；Shell 的 127 则表示命令未找到。

## 验证状态

隔离构建验证通过：生成 ARM 静态程序；main.d 包含 main.c、io_utils.h、app_message.h，io_utils.d 只包含 io_utils.c、io_utils.h。更新 app_message.h 后，预览与实际构建均只编译 main.c 并链接；更新 io_utils.h 后，预览与实际构建均编译两个模块并链接。预先验证位于 .validation/project。

学习者已亲手完成：首次编译状态 0；查看的 .d 内容符合实际包含关系；touch app_message.h 后预览及实际执行两条命令；touch io_utils.h 后执行三条。学习者明确表示原理仍不清楚，当前进入详细讲解阶段，不能把命令成功等同于理解掌握。

参考：[GCC 依赖生成选项](https://gcc.gnu.org/onlinedocs/gcc/Preprocessor-Options.html)。
