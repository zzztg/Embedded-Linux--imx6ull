# 实验 02：进程与信号

配套：[进程与信号理论笔记](理论笔记.md)。

## 已观察到的结果

- `sleep 60 &` 在后台运行；`$!` 得到它的 PID，`jobs` 显示当前 Shell 的后台任务。
- 对新建的 `sleep 300` 发送 SIGTERM，`wait` 返回 143，即 Bash 使用的 128 + 15。

## C 程序的实验目标

`signal_demo.c` 注册 SIGINT 和 SIGTERM 的处理函数。收到信号时，处理函数记录信号编号；主循环随后退出，打印结果，并从 `main` 返回 0。

信号处理函数只修改 `volatile sig_atomic_t` 标志。`printf` 等普通操作放在主流程里，避免在异步信号处理中调用它们。

程序用 `sleep(1)` 等待，每秒检查标志，避免一直空转。此例只演示信号通知；更精确的等待机制以后再学习。

当前验证：ARM 静态版本已编译通过；电脑端版本分别收到 SIGTERM 和 SIGINT 后，打印了正确的信号编号，退出状态均为 0。

2026-10-02 已完成开发板验证：PID 为 26827 的程序收到 SIGTERM 后打印 `Received signal 15; exiting normally.`，`wait` 取得的退出状态为 0。

后台进程的打印可能与终端上正在输入的命令交错显示。本次 `study_pid=$!` 与 `PID=26827` 的显示交错，但后续发送信号和退出状态均已验证正确。

## WSL-Ubuntu：编译与传输

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/02_signals
arm-linux-gnueabihf-gcc -std=c11 -Wall -Wextra -static signal_demo.c -o signal_demo
scp -O signal_demo root@192.168.10.2:/root/linux-study/02_signals/
```

传输沿用实验 01 的直连网络配置。重启开发板后需重新设置 eth0 地址。

## COM12：运行和发送信号

```sh
cd /root/linux-study/02_signals
chmod +x signal_demo
./signal_demo &
study_pid=$!
```

等程序打印 `PID=...; waiting for SIGINT or SIGTERM...` 后，再执行：

```sh
kill "$study_pid"
wait "$study_pid"
echo $?
```

预期打印 `Received signal 15; exiting normally.`，最后退出状态为 0。

这个上板预期已经验证通过。

必须等程序打印准备完成的提示后再发送信号；如果信号在处理函数安装前到达，默认动作可能会直接终止进程。

## 延伸实验：前台运行与 Ctrl+C

在 COM12 的 `/root/linux-study/02_signals` 中执行：

```sh
./signal_demo
```

等程序打印准备完成的提示后，按 Ctrl+C，再执行：

```sh
echo $?
```

预期打印 `Received signal 2; exiting normally.`，退出状态为 0。

2026-10-02 已完成开发板验证：前台进程 PID 为 4098，按 Ctrl+C 后收到 SIGINT（编号 2），打印上述信息，退出状态为 0。

## 核心概念

- 可执行程序是文件；启动后成为进程。
- 任务号由当前 Shell 管理；PID 是进程标识。
- `kill` 默认发送 SIGTERM；收到信号后的行为由信号种类、默认动作和程序设置决定。
- 信号处理函数不是单片机的硬件中断服务函数，它运行在用户进程中。

参考：POSIX sigaction https://pubs.opengroup.org/onlinepubs/9799919799/functions/sigaction.html

Linux sigaction 手册：https://man7.org/linux/man-pages/man2/sigaction.2.html

信号处理中的函数安全性：https://man7.org/linux/man-pages/man7/signal-safety.7.html
