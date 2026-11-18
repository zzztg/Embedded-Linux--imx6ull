# 实验 04-01：多文件编译与第一份 Makefile

本节沿用熟悉的文件写入行为，把主流程和通用写入函数拆成模块。先手动生成两个目标文件再链接，随后用 Makefile 表达相同的依赖与命令。学习者需亲手完成编译。

## 文件职责

| 文件 | 职责 |
|---|---|
| main.c | 参数检查、打开文件、调用 write_all、关闭文件 |
| io_utils.h | 声明 write_all 的名称、参数与返回值类型 |
| io_utils.c | 定义 write_all 的函数体 |
| Makefile | 描述如何生成 .o 和最终程序 |

源码：[main.c](main.c)、[io_utils.h](io_utils.h)、[io_utils.c](io_utils.c)。

声明告诉编译器怎样检查调用，不提供函数实现。定义包含函数体。main.c 包含头文件后可以编译成 main.o，但链接时仍需要 io_utils.o 提供 write_all 的定义。

以前单文件示例中的 write_all 是 static 函数，限定于该编译单元；本节需要跨模块调用，因此声明和定义使用外部链接，不加 static。这里的 static 与信号标志变量的存储期问题应按使用位置区分。

头文件中的 #ifndef/#define/#endif 是包含保护，防止同一个头文件在一次预处理过程中重复展开。main.c 和 io_utils.c 分别包含同一头文件是正常的；实现文件也包含自己的头文件，便于编译器检查声明和定义是否一致。

## 编译流程

```text
main.c     ──预处理、编译、汇编──→ main.o
io_utils.c ──预处理、编译、汇编──→ io_utils.o
                                      ↓ 链接两个目标文件和所需库
                                   file_app
```

预处理展开 #include 等指令；编译将 C 转为汇编；汇编产生机器代码形式的目标文件；链接组合目标文件和库、解析符号引用、生成可执行程序。GCC 驱动可以连续调用这些工具。

- -E 停在预处理结果。
- -S 停在汇编文本。
- -c 完成目标文件生成，停止在链接之前。
- 不使用这些停止选项时，GCC 通常继续链接。

.o 通常是可重定位目标文件，包含代码、数据、符号和重定位信息；本实验不能把它当可执行程序运行。两个 .c 最后形成同一个程序，不是两个进程。

## WSL-Ubuntu：先亲手分步编译

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/04_build/01_multi_file
arm-linux-gnueabihf-gcc -std=c11 -Wall -Wextra -c main.c -o main.o
arm-linux-gnueabihf-gcc -std=c11 -Wall -Wextra -c io_utils.c -o io_utils.o
arm-linux-gnueabihf-gcc -static main.o io_utils.o -o file_app
echo $?
file main.o io_utils.o file_app
```

每条编译命令应成功；任一步报错时先处理该步，再继续后面的命令。最后的 echo 检查链接命令状态。预期两个 .o 是 ARM relocatable，file_app 是 ARM 静态可执行 ELF。

另一种写法 `arm-linux-gnueabihf-gcc -std=c11 -Wall -Wextra -static main.c io_utils.c -o file_app` 也会编译并链接，但本节先分步观察 .o。

## 板端创建目录，然后从 WSL 传输

COM12：

```sh
mkdir -p /root/linux-study/04_build/01_multi_file
```

WSL：

```sh
scp -O file_app root@192.168.10.2:/root/linux-study/04_build/01_multi_file/
```

板端 IP 沿用直连配置，重启后需按学习路线重新设置。只传输可执行程序即可，本节板端运行不需要 .o 或 .h。

COM12：

```sh
cd /root/linux-study/04_build/01_multi_file
chmod +x file_app
./file_app app_note.txt
echo $?
cat app_note.txt
ls -l app_note.txt
```

预期打印 `Wrote 27 bytes using io_utils.c`，退出状态为 0，文件大小为 27，内容与第三章的写入实验一致。

## 下一步：让 make 管理这三条命令

2026-10-13 环境检查发现 WSL 尚未安装 make。进入本步骤时，由学习者在 WSL 安装并检查：

```sh
sudo apt update
sudo apt install make
make --version
```

make 是构建工具，GCC 是编译器。Makefile 则是供 make 读取的规则文件。

规则结构是：

```make
目标: 依赖文件
	构建目标的命令
```

命令行开头是真正的 Tab。目标不存在，或依赖文件比目标新时，make 会按依赖关系运行对应命令；依赖目标需要更新时先更新依赖。第一份 Makefile 的默认目标是 file_app。

main.o 和 io_utils.o 都依赖 io_utils.h：头文件变化也会影响编译。普通 make 主要依据文件存在性与时间戳，不自动比较内容，也不会仅因编译命令文本改变就必然重编译。

亲手分步编译后运行 make，通常提示无需更新。make 根据依赖关系先更新需要更新的依赖，再执行目标的配方。默认目标是第一个普通目标 file_app，执行顺序由依赖关系决定。

### 亲手观察四种情况（均在 WSL 工程目录）

先执行 `make clean` 再执行 `make`：两个 .o 和最终程序都不存在，应执行两条编译、一条链接，共三条构建命令。clean 配方仅删除这三个已命名的生成文件，不删除源码或 app_note.txt。

紧接着再执行 `make`：文件没有变化，应提示 file_app 已是最新状态，不重复执行编译和链接。

然后逐条执行：

```sh
sleep 1
touch io_utils.c
make
```

touch 只更新时间戳，不更改文件内容。此时应执行 io_utils.c 的编译和最终链接，共两条命令；main.o 不依赖 io_utils.c，因此不需要重编译。

最后逐条执行：

```sh
sleep 1
touch io_utils.h
make
```

两个 .o 都依赖 io_utils.h，因此应执行两条编译和一条链接，共三条命令。sleep 用于让时间戳差异足够清晰。四组观察已由学习者完成，结果为 3、0、2、3 条构建命令。

clean 是清理已命名生成文件的动作，.PHONY 避免目录里名为 clean 的文件影响这个动作。make clean 仅删除 main.o、io_utils.o、file_app，源码和实验数据保留。执行清理后再 make 才会看到完整三条构建命令。

## 验证状态

教师预先验证通过：电脑端两个 .o 成功链接，程序写入 27 字节，文件内容逐字节核对一致；两个 ARM .o 均为可重定位目标文件，链接产物为 ARM 静态可执行 ELF。教师验证文件保存到 .validation。

2026-10-20 学习者已亲手完成两次 ARM 编译和最终链接，链接退出状态为 0；file 检查显示 main.o、io_utils.o 为 ARM relocatable，file_app 为 ARM executable、statically linked。一次粘贴混入终端控制字符的链接命令失败，重新输入正常命令后成功；不是编译器缺失。开发板验证结果见下文。

开发板运行已完成验证：程序打印 Wrote 27 bytes using io_utils.c，退出状态为 0；cat 显示预期文字，ls -l 显示 app_note.txt 大小为 27 字节。

2026-10-21 学习者已在 WSL 实际运行 make。清理后完整构建执行三条命令，退出状态 0；再次 make 提示 file_app 已是最新状态；touch io_utils.c 后仅编译 io_utils.c 并链接；touch io_utils.h 后两个模块均重编译再链接。四组观察全部符合预期，证明 make 工具已可用。一次 sleep 缺少参数的输入已改为 sleep 1，不影响后续观察。

参考：[GCC 编译阶段与停止选项](https://gcc.gnu.org/onlinedocs/gcc/Overall-Options.html)。
