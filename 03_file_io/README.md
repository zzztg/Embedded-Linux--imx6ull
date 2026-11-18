# 实验 03：文件 I/O

先阅读：[文件 I/O 理论笔记](理论笔记.md)，再结合下文进行上板实验。

## 目标

在开发板的 `/root/linux-study/03_file_io/02_write` 目录中，通过 C 程序创建 `note.txt`，写入一行文字，并查看文件内容和权限。

## 整理后的目录

2026-10-12 起，以下操作步骤使用新的分节目录。此前验证记录描述的是当时的执行结果。

| 分节 | WSL 路径（位于 /mnt/c/Users/dingf/Desktop/imx6ull 下） | 板端路径（位于 /root/linux-study 下） |
|---|---|---|
| 描述符与重定向 | 无 C 源码 | 03_file_io/01_fd |
| 写入 | 03_file_io/02_write | 03_file_io/02_write |
| 读取 | 03_file_io/03_read | 03_file_io/03_read |
| 定位与覆盖 | 03_file_io/04_seek | 03_file_io/04_seek |
| 追加 | 03_file_io/05_append | 03_file_io/05_append |
| 缓冲 | 03_file_io/06_buffer | 03_file_io/06_buffer |

源码：[写入](02_write/file_write.c)、[读取](03_read/file_read.c)。定位、追加和缓冲的步骤分别见 [文件位置实验](文件位置实验.md) 和 [缓冲实验](缓冲实验.md)。

本实验认识 `open`、`write`、`close` 和文件描述符。`open` 返回一个整数描述符，后续使用这个描述符操作已打开的文件。

标准输入、标准输出、标准错误通常分别使用描述符 0、1、2；新打开文件的描述符通常是 3，但应以实际返回值为准。

## 已观察：串口终端与描述符

开发板串口中，`echo $$` 得到当前 Bash 的 PID 为 343。`ls -l /proc/$$/fd` 显示描述符 0、1、2、255 均指向 `/dev/ttymxc0`。

这说明当前 Shell 的标准输入、标准输出和标准错误连接到同一个串口终端。255 是 Bash 内部使用的描述符；它不意味着 3 到 254 已被占用。这里观察的是 Shell 的描述符表，随后启动的 C 程序有自己的描述符表，通常继承标准输入、标准输出和标准错误。

参考：[Linux 的进程描述符视图](https://man7.org/linux/man-pages/man5/proc_pid_fd.5.html)。

开发板重定向实验也已验证：`echo "normal output" > fd_stdout.txt` 的文字在读取文件时显示；单独执行 `ls /root/linux-study/no_such_file_for_fd_demo 2> fd_stderr.txt` 时，错误信息进入文件，通过 `cat fd_stderr.txt` 读出。这分别对应标准输出（描述符 1）和标准错误（描述符 2）的重定向。

## WSL-Ubuntu：编译与传输

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/03_file_io/02_write
arm-linux-gnueabihf-gcc -std=c11 -Wall -Wextra -static file_write.c -o file_write
echo $?
file file_write
scp -O file_write root@192.168.10.2:/root/linux-study/03_file_io/02_write/
```

沿用此前的直连网络。开发板重启后，需通过 COM12 重新设置：

```sh
ifconfig eth0 192.168.10.2 netmask 255.255.255.0 up
```

## COM12：执行和检查

每条命令分别输入并按回车：

```sh
cd /root/linux-study/03_file_io/02_write
chmod +x file_write
./file_write note.txt
echo $?
cat note.txt
ls -l note.txt
```

预期程序打印文件描述符、写入的字节数和文件名，退出状态为 0。`cat` 读出的内容为：

```text
Hello from Linux file I/O!
```

`echo $?` 必须紧接程序执行，才是在查看这个程序的退出状态。

## 代码中的重要细节

- `argv[1]` 是命令行中提供的目标文件名，本实验为 `note.txt`。
- `O_WRONLY`：以只写方式打开。
- `O_CREAT`：目标文件不存在时创建它；不会自动创建父目录。
- `O_TRUNC`：目标文件存在时清空其内容。因此仅使用学习目录中的实验文件。
- `0644`：创建文件时请求的权限，实际权限还受进程的 `umask` 影响。
- 写入长度不包含 C 字符串末尾的 `\0`，但包含文字中的换行。
- `write` 返回实际写入的字节数，不保证一次完成所有数据，因此 `write_all` 循环处理；被信号打断时重试。
- `close` 释放文件描述符。

当前验证：ARM 静态版本编译通过；电脑端版本创建了文件并写入 27 字节，文件内容逐字节核对通过。开发板写入实验已验证：`open` 返回描述符 3，程序报告写入 27 字节，退出状态为 0；`cat note.txt` 显示预期文字，`ls -l note.txt` 显示普通文件、权限 `-rw-r--r--`、所有者和所属组均为 root、大小 27 字节。

错误处理实验已在开发板验证：不提供文件名时，打印用法并返回 2；提供不存在的父目录时，`open` 打印 `No such file or directory`，程序返回 1。

## 读取实验：16 字节缓冲区与 EOF

源码：[file_read.c](03_read/file_read.c)。程序只读打开输入文件，每次最多读取 16 字节。读取数量日志写向标准错误；读到的原始字节写向标准输出，因此可以通过重定向把两者分开。

WSL 中编译与传输：

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/03_file_io/03_read
arm-linux-gnueabihf-gcc -std=c11 -Wall -Wextra -static file_read.c -o file_read
echo $?
file file_read
scp -O file_read root@192.168.10.2:/root/linux-study/03_file_io/03_read/
```

COM12 中逐条执行：

```sh
cd /root/linux-study/03_file_io/03_read
chmod +x file_read
./file_read ../02_write/note.txt > read_copy.txt
echo $?
cat read_copy.txt
ls -l ../02_write/note.txt read_copy.txt
```

对当前 27 字节普通文件，典型日志为读取 16、11、0 字节。16 和 11 是有效数据量；下一次返回 0 才报告当前位置已到 EOF。程序不能把短读当成结束，必须处理每次实际读到的数量。这里读取长度始终大于 0。

注意读取节没有 note.txt：`../02_write/note.txt` 表示先从 03_read 返回上一级 03_file_io，再进入相邻的 02_write。read_copy.txt 则在当前 03_read 目录创建。

`read` 不自动补字符串结束符，第二次只有缓冲区前 11 字节有效，后 5 字节可能保留上一次的数据。输出使用实际返回长度，避免写出旧数据；遇到数据中的零字节，也不会提前停止。

读取示例已完成电脑端验证：空文件、恰好 16 字节的文件、27 字节文本和包含零字节的二进制文件，输出均与输入逐字节一致；27 字节文本的日志为 16、11、0；缺少参数与路径不存在分别返回 2、1。ARM 静态版本编译通过。

开发板读取实验已验证：`./file_read note.txt > read_copy.txt` 的读取日志依次为 16、11、0；程序退出状态为 0；`cat read_copy.txt` 显示预期文字。注意 `read` 的返回值 0 表示 EOF，主函数返回 0 表示本程序正常结束，二者属于不同层次。

在这个具体例子中，第一次读入 `Hello from Linux`（16 字节）；第二次读入 ` file I/O!\n`（11 字节），缓冲区后 5 字节仍保留上次的 `Linux`。如果第二次错误地输出整个 16 字节缓冲区，就会多输出这 5 个旧字节。实际代码按本次 count 输出。

可用空文件补充观察：在板端 `03_file_io/03_read` 目录执行 `printf '' > empty_read_demo.txt`（创建或清空这个实验文件），随后运行 `./file_read empty_read_demo.txt > empty_read_copy.txt`。首次 read 返回 0，程序成功结束，输出文件为空。空文件不是读取失败。此补充实验的运行结果尚待学习者提供。

参考：
- https://man7.org/linux/man-pages/man2/open.2.html
- https://man7.org/linux/man-pages/man2/write.2.html
- https://man7.org/linux/man-pages/man2/close.2.html
