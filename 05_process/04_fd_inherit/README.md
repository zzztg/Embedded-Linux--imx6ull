# 实验 05-04：继承的文件描述符与共享偏移

状态：学习者已完成两种模式的板端验证，退出状态均为 0。shared 模式父 PID 23359、fork 前 fd=3；子读 ABCD（0→4），close 后把局部 fd 改为 -1；父回收后局部 fd 仍为 3，读 EFGH（4→8）。separate 模式父 PID 23603、fork 前 fd=-1；子独立 open 后读 ABCD（0→4），父回收后局部 fd 仍为 -1，随后独立 open 返回 3，也读 ABCD（0→4）。继续原理理解检查；尚未收到本节 WSL 编译输出，不据此标记编译检查。05-03 两条路径已上板验证，学习者已正确解释：直接 exec 成功后不保留独立启动器，也不会执行旧程序后续 printf。

教师隔离验证通过：生成 ARM 静态程序；WSL 本机版本以 -Wall -Wextra -Werror 编译。shared 模式子读 ABCD（0→4），子 close 后父 fd 保持可用并读 EFGH（4→8）；separate 模式双方独立从 0 读 ABCD；两种模式均退出 0，输入文件内容不变。此验证不替代学习者亲手编译或板端运行。

## 三层模型

文件描述符 fd 是进程内的整数索引。每个普通 fork 后的进程有各自的文件描述符表；表项引用内核的打开文件描述（open file description），其中记录当前文件偏移、文件状态标志，并关联实际文件。

~~~text
父进程普通变量 fd=3 → 父描述符表[3] ─┐
                                     ├→ 同一打开文件描述 → 文件
子进程普通变量 fd=3 → 子描述符表[3] ─┘   offset、O_APPEND 等
~~~

fork 复制描述符表的引用关系，不为每个继承的描述符重新 open 文件。父子表项引用同一打开文件描述，所以其偏移是共享状态。普通变量 fd 的副本仍互不影响；变量中并不存放文件偏移。

## 本节两种模式

- shared：fork 前 open 一次，子进程读 4 字节后 close 并退出；父进程 waitpid 后再读。两次读使用同一个打开文件描述，预期子读 ABCD，父读 EFGH。
- separate：fork 前 fd=-1，子进程自行 open、读、close、退出；父进程 waitpid 后另行 open 同一路径，再读。两次 open 建立独立的打开文件描述，预期双方都读 ABCD。

本节显式用 waitpid 保证子先读、父后读，不用 sleep 猜调度；等待只保证执行顺序，不会自动修改、重置或复制文件偏移。输入用普通可定位文件，且不在实验中被其他程序修改。

## 为什么子 close 后父仍可读

子进程 close(fd) 关闭的是子进程表中的那个描述符，父进程自己的表项仍然引用内核对象。子进程再执行 fd=-1 只是修改自己的 C 变量，不会关闭父进程的描述符，也不会改变父变量。

最后一个引用释放时，相应打开文件描述的资源才可释放。close 不会把普通文件内容删除，也不因为关闭一个引用就把共享偏移归零。

即使父子两次独立 open 返回的整数都为 3，也不能据此认定它们共享偏移：同一个数字在不同进程里只是各自表中的索引。本节 common case 为 fd=3，但不可把 3 写死为接口保证。

两次 open 同一路径仍可指向同一个实际文件，独立的是打开状态。父子独立偏移不等于文件内容被复制成两份。

## 代码阅读点

read_piece 使用 read 而不是 fread，避免 C 库预读缓冲影响本次偏移观察。lseek(fd, 0, SEEK_CUR) 只查询当前偏移；read 最多读 4 字节，用返回值控制打印长度。read 允许短读和 EOF；对本节 16 字节的未变动普通文件，预期每次获得 4 字节。

char data[4] 不要求额外的字符串结束字符，因为 printf 的 %.*s 明确限制最多输出 count 个字符；本节输入是无内嵌 NUL 的 ASCII 文本，不把该打印形式推广为任意二进制数据输出方式。

子分支在 main 中 return，以正常刷新自己的输出；fork 前刷新已有 stdout 防止重复。父进程验证子正常退出且为 0 后才继续读。read 与 waitpid 遇到 EINTR 重试；close 不盲目重试。

## WSL：阅读和亲手编译

~~~sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/05_process/04_fd_inherit
cat src/fd_inherit.c
make -n
make
echo $?
file build/fd_inherit
~~~

预期生成 ARM 32 位静态程序。

## 传输

COM12 创建目录：

~~~sh
mkdir -p /root/linux-study/05_process/04_fd_inherit
~~~

WSL 当前目录：

~~~sh
scp -O build/fd_inherit root@192.168.10.2:/root/linux-study/05_process/04_fd_inherit/
~~~

## COM12：继承同一个打开状态

~~~sh
cd /root/linux-study/05_process/04_fd_inherit
chmod +x fd_inherit
printf 'ABCDEFGHIJKLMNOP' > input.txt
./fd_inherit shared input.txt
echo $?
~~~

预期关键输出（PID 和 fd 按实际运行）：

~~~text
mode=shared, before fork: pid=..., fd=3
child: fd=3, offset=0 -> 4, read=4, data="ABCD"
child: closed its fd; local fd=-1
parent: child reaped; local fd=3
parent: fd=3, offset=4 -> 8, read=4, data="EFGH"
~~~

退出状态应为 0。共享偏移不代表普通变量 fd 共享；父仍能读也说明子 close 没有关闭父的描述符。

## COM12：分别 open 的对照

~~~sh
./fd_inherit separate input.txt
echo $?
cat input.txt
~~~

预期子读 ABCD，偏移 0→4；父的局部 fd 在子退出后仍是 -1，父自行 open 后再从 0 读 ABCD，偏移 0→4，退出状态为 0。cat 显示完整 ABCDEFGHIJKLMNOP，因为 read/close 不改变文件内容。本节输入末尾没有换行，Shell 提示符可能紧跟在 P 后面。

## 边界与后续

相同 fd 数字不保证引用同一内核对象；不同数字也不保证独立，后续 dup/dup2 会让同一进程中的两个描述符引用同一打开文件描述。

文件状态标志如 O_APPEND 关联打开文件描述；FD_CLOEXEC 等描述符标志关联各自表项。FD_CLOEXEC 在 fork 后仍可作为子描述符的标志存在，但只有成功 exec 时才触发关闭，不是 fork 时关闭。本节没有 exec，相关观察安排在后续实验。

文件描述符指向管道、终端等对象时，lseek 可能失败；不能把本例普通文件的偏移模型直接套到所有描述符。

参考：[fork 描述符继承](https://man7.org/linux/man-pages/man2/fork.2.html)、[open 与打开文件描述](https://man7.org/linux/man-pages/man2/open.2.html)、[close](https://man7.org/linux/man-pages/man2/close.2.html)。
