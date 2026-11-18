# 实验 06-02：通过管道取得子程序的标准输出

状态：学习者已完成直接转发、外层 stdout 保存到文件及目标不存在三条板端验证。直接运行父 PID 20875、子 PID 20877，捕获 117 字节，EOF 后回收取得子退出码 7；保存文件运行父 PID 24743、子 PID 24744，父诊断与子 stderr 留在串口，replay.txt 只包含目标 stdout，wc -c 为 117，与本轮 captured 计数一致，父退出 0。目标缺失路径父 PID 23418、子 PID 23419，child execl 报 No such file or directory，父捕获 0 字节、得到 EOF、回收子退出码 127，父退出 0。继续 EOF 与进程终止的理解检查；尚未收到本节 WSL 编译输出，不据此标记编译检查。上一节已正确推演：父先 wait、后关闭写端，而子等 EOF 才退出，会产生相互等待的死锁；父阻塞在 waitpid，子阻塞在 read，并非两个忙循环。

教师隔离验证通过：两个 ARM 静态程序均构建成功；WSL 本机版本以 -Wall -Wextra -Werror 编译。验证了子 stdout 的 PID/参数、父捕获计数与实际字节数一致、stderr 未混入捕获流、父 stdout 保存到文件的同样效果；缺失目标路径下捕获 0 字节、EOF 后回收子退出 127；另用测试写入程序验证完整捕获 1048576 字节并完成回收。父在这些情形中按教学约定退出 0。此验证不替代学习者亲手编译或板端运行。

## 从文件重定向到管道

上一节方向为父写、子读。本节方向反过来，由外部子程序写标准输出，父进程读取：

~~~text
args_demo 的 printf → 子[1] → 管道写端 → 内核缓冲 → 父的管道读端
                                                           ↓ read
                                                    父自己的 buffer
                                                           ↓ write
                                                    父[1] 原标准输出
~~~

子程序无须直接知道 pipe 的存在。子进程在 exec 前 dup2(pipefd[1], 1)，exec 成功后新程序使用继承的描述符 1。旧 C 库 FILE 和缓冲不跨 exec 保留，新程序初始化自己的标准流。

## 端点必须正确关闭

fork 后父子都有读写端副本，分别处理：

| 父进程 | 子进程 |
|---|---|
| 关闭写端，只保留读端 | 关闭读端 |
| 循环 read 管道 | dup2(原写端, 1) |
| 读到 EOF，关闭读端 | 关闭原写端编号，保留描述符 1 的写引用 |
| waitpid 回收 | exec，新程序通过 1 写入管道 |

通常原管道读端为 3、写端为 4；dup2 后子进程的 1 和 4 同时引用写端。close(4) 不关闭 1；新程序结束时，1 对应的最后写引用释放，父读完剩余数据后得到 EOF。

如果父忘关自己的写端，即使子退出也得不到 EOF。如果子又 fork 出其他进程且它们继承并保留写端，EOF 也可能延迟；本例目标没有创建后代。

## 为什么父进程先读、后 wait

本节循环读取使父子在执行期间能持续传输。若父先 wait 子退出、之后才 read，少量输出可能碰巧完成，但输出超过管道可用空间时，子会阻塞在写入，父却阻塞在等待子退出，形成死锁。

这个死锁与上一节的 EOF 等待不同：上一节子在等最后一个写端关闭；本节错误顺序可能让子在等读者腾出缓冲空间。不要因为短输出实验没有卡住，就把错误顺序当成正确实现。

EOF 与进程退出也不是同一件事：子可以先关闭 stdout，再做其他工作，此时父可能先读到 EOF，仍须 waitpid 等待进程真正结束并回收。

## 父进程怎样处理 read 的字节

本例用 32 字节数组循环 read，按每次返回的实际长度累计 total，再用 write_all 把这批字节原样送到父的标准输出。没有在 read 返回的数据后添加 NUL，也不按字符串猜长度；read 分块不保证等于输出行或 printf 次数。

捕获不等于必须存文件或一次性保存整个输出。本例父已经取得字节，只是选择实时转发；实际应用可以改为解析、记录或累计到有上限的缓冲。本例没有额外的输出文件路径参数。

## 为什么父诊断写 stderr

pipe_exec 的 PID、端点编号、EOF 总字节数和回收状态都使用 stderr。父 stdout 只原样转发捕获的子 stdout，便于对照：

- 直接运行时，子 stdout 经父转发后显示在终端，子 stderr 和父诊断也显示在终端，但通路不同。
- 对启动器加 > replay.txt 时，文件只包含父转发的子 stdout，诊断继续留在终端。

终端中看到了 args_demo 的内容，并不说明子进程直接写串口；要结合描述符设置及父读取代码判断数据通路。

## 缓冲与状态

目标 args_demo 保持前一节代码，printf PID/参数并 return 7；它的 stdout 指向管道，通常全缓冲，正常从 main 返回会刷新。本例可能直到目标退出时才看到部分输出，不据此判断管道没有传输能力。需要实时输出的目标应根据需求主动 fflush 或调整缓冲，捕获方不能替它刷新用户空间缓冲。

父在 fork 前没有向 stdout 写入待刷新的 C 库内容，父的转发使用底层 write_all，不混用 stdio 的 stdout 缓冲。父子 diagnostics 均为 stderr，未重定向。

启动器继续沿用教学约定：报告子退出 7 或 exec 失败后的 127，不自动把它们传播为自己的退出码；父返回的是读取/转发/回收流程的结果，正常为 0。126/127 是子失败分支主动设置的状态，不是系统调用返回值。

## WSL：阅读与亲手编译

~~~sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/06_ipc/02_pipe_exec
cat src/pipe_exec.c
cat src/args_demo.c
make -n
make
echo $?
file build/pipe_exec build/args_demo
~~~

预期生成两个 ARM 32 位静态程序。

## 传输

COM12 创建目录：

~~~sh
mkdir -p /root/linux-study/06_ipc/02_pipe_exec
~~~

WSL 当前目录传输：

~~~sh
scp -O build/pipe_exec build/args_demo root@192.168.10.2:/root/linux-study/06_ipc/02_pipe_exec/
~~~

## COM12：直接捕获与转发

~~~sh
cd /root/linux-study/06_ipc/02_pipe_exec
chmod +x pipe_exec args_demo
./pipe_exec ./args_demo
echo $?
~~~

预期看见 child setup 和 new program 中相同子 PID，父读到 EOF 时显示 captured 若干字节，随后回收取得子退出码 7，父退出 0。总字节数包含子 stdout，排除父诊断和子 stderr，具体数字随 PID 位数等改变，不写死。

注意 EOF 行可能在新程序 stderr 行之后或显示不同交错，输出先后受调度和缓冲影响；只固定本进程内代码的执行关系。

## COM12：把父的转发输出保存到文件

~~~sh
./pipe_exec ./args_demo > replay.txt
echo $?
cat replay.txt
wc -c replay.txt
~~~

运行时仍看到父诊断和子 stderr，新程序 stdout 在 cat 时才显示。wc -c 的字节数应与同一次运行中 parent: EOF, captured N bytes 的 N 相等，不能与上一轮不同 PID 时的数字强行对比。文件只包含 args_demo 的 stdout，不应包含 parent 或 stderr 诊断。

## COM12：目标不存在的对照

~~~sh
./pipe_exec ./missing_exec_parent_for_demo/args_demo
echo $?
~~~

前提是该父目录不存在。预期 child execl 报错留在 stderr，父仍可读到 EOF，captured 0 bytes，回收子退出码 127，父返回 0。因为子已将 1 接上写端，失败时 _exit 会关闭它，父也已经关闭自己的写端，故不会永久等待。

## 边界与后续

本节只捕获 stdout。若希望把 stderr 也接同一管道，可在子进程中另外 dup2 到 2，但会混合两类输出；若分别建立两条管道，则需要同时处理两个读端，不能简单读完一个再读另一个，后续结合 poll 等 I/O 多路复用学习。

外层输出接收者若提前关闭管道，父 write 可能触发 SIGPIPE，默认会终止父进程；本例没有安装 SIGPIPE 处理，也不是完整的通用进程管理器。后续学习错误传播和信号处理时再补充这一方向。本节不触发该错误。

只有一条子→父数据通路；双向通信不能把同一普通管道当成双向通道。后续还会学习管道容量、非阻塞、协议边界和更复杂同步，不在本节把所有机制叠加。

参考：[管道容量、EOF 与阻塞](https://man7.org/linux/man-pages/man7/pipe.7.html)、[dup2](https://man7.org/linux/man-pages/man2/dup.2.html)、[execve](https://man7.org/linux/man-pages/man2/execve.2.html)。
