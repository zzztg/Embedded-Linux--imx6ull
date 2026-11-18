# 实验 04-02：Makefile 变量与编译选项

沿用 04-01 的三个 C 源码文件及相同的依赖关系。本节用变量集中管理编译器、选项和文件名，随后观察命令行变量与时间戳构建的关系。新目录中的学生编译产物由学习者亲手生成。

## 变量

```make
CC := arm-linux-gnueabihf-gcc
CFLAGS := -std=c11 -Wall -Wextra
LDFLAGS := -static
TARGET := file_app
OBJECTS := main.o io_utils.o
```

这些是 make 的文本变量。:= 在读取赋值时展开右侧并保存结果；$(CC) 等引用由 make 展开，再将配方交给 Shell 执行。具体名称与使用位置由 Makefile 决定，CC/CFLAGS/LDFLAGS 是常见约定。TARGET、OBJECTS 是本工程选用的变量名。

| 变量 | 本节用途 |
|---|---|
| CC | 调用哪个编译器驱动 |
| CFLAGS | 传给 C 编译器驱动的选项，本节编译和链接命令均带上 |
| LDFLAGS | 链接选项，如 -static |
| TARGET | 最终程序名 |
| OBJECTS | 链接所需目标文件列表 |

all 是默认的汇总动作目标，依赖 TARGET。它没有自己的配方，make 先更新所需的程序。显式 .PHONY 标记 all、clean；后续再次 make 提示 Nothing to be done for 'all' 也是正常的无更新状态。

## 亲手构建（全部在 WSL）

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/04_build/02_make_variables
make
echo $?
file file_app
```

预期打印两次编译和一次链接，状态为 0，file_app 是 ARM 静态可执行程序。文件写入行为已在 04-01 上板验证，本节观察构建规则和选项。

## 改变量为什么可能没有重编译？

先执行：

```sh
make CFLAGS='-std=c11 -Wall -Wextra -O2'
```

命令行赋值覆盖 Makefile 中的普通赋值，整个值被替换，不会自动追加。引号让包含空格的选项列表作为一个赋值参数传给 make。-O2 是 GCC 优化级别，本节只观察它是否进入构建命令，优化细节后续展开。

这条命令不会把新值写回 Makefile，也不会永久设置当前 Shell 的 CFLAGS。下一次普通 make 重新按 Makefile 定义取值，但既有产物不会仅因此自动重新生成。

如果刚完成上面的默认构建，且源文件和头文件都未变化，此次可能没有任何编译命令。普通 make 不自动记录比较上次构建时的 CFLAGS；变量改变本身没有成为本例依赖文件的时间戳变化。

接着执行：

```sh
make clean
make CFLAGS='-std=c11 -Wall -Wextra -O2'
echo $?
```

生成文件已删除，构建会重新执行，命令中应出现 -O2。本节用清理后重建让编译选项生效。自动跟踪构建参数需要进一步规则或构建系统支持。

make clean 在本目录只删除本节 OBJECTS 和 TARGET 指定的生成文件。

## 只查看计划

`make -n -B CFLAGS='-std=c11 -Wall -Wextra -O2'` 可以打印强制重建时的普通配方，供检查变量展开；本 Makefile 没有递归 make 等特殊配方，因此此命令不执行这些编译操作。-n 是打印计划，-B 是将目标视为需要重建。本步骤可在理解实际构建后练习。

## 后续

接下来再学习 $@、$<、$^ 和 %.o 等自动变量、模式规则；本节用明确规则保持依赖关系可读。

## 验证状态

教师隔离构建验证通过：默认规则生成 ARM 静态程序；目标已最新时，make -q 确认仅改变 CFLAGS 不触发重建；强制打印构建计划时，两次编译和一次链接均正确展开 -O2。预先验证位于 .validation/project。

学习者已完成三组观察：默认构建生成 ARM 静态程序，状态 0；覆盖 CFLAGS 后两次 make 均提示 all 无需更新；清理后使用新 CFLAGS，两次编译和一次链接都含 -O2，最终状态 0。变量覆盖与构建更新条件均已实际验证。
