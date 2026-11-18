# 实验 01：在 i.MX6ULL 上运行第一个 C 程序

状态：已在开发板上验证成功。

## 环境

- 开发板：100ASK i.MX6ULL Pro。
- 板端系统：Linux 4.9.88，ARMv7 小端，Buildroot 2020.02。
- 电脑端开发环境：Ubuntu 26.04 LTS，WSL2，x86_64。
- 串口：COM12，115200，8 数据位，1 停止位，无校验、无流控。
- 网线直连使用的地址：电脑有线网口 192.168.10.1/24，开发板 eth0 为 192.168.10.2/24。
- Windows 有线网卡名称：以太网（Realtek PCIe GbE Family Controller）。

## 1. 在 WSL-Ubuntu 中安装工具

```sh
sudo apt update
sudo apt install gcc-arm-linux-gnueabihf libc6-dev-armhf-cross file
```

`sudo` 提示的是 Ubuntu 用户的密码，输入时不显示字符。

## 2. 在 WSL-Ubuntu 中编译

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/01_hello
arm-linux-gnueabihf-gcc -Wall -Wextra -static hello.c -o hello
file hello
```

验证结果包含 `ELF 32-bit`、`ARM`、`EABI5`、`statically linked` 和 `for GNU/Linux 3.2.0`。

`-static` 把所需的库代码链接进程序，减少对板子运行库版本的依赖。它不消除对 CPU 架构和内核接口的要求。本程序已实际运行验证。

## 3. 配置直连网络

Windows 的“以太网”IPv4 地址为 192.168.10.1，子网掩码为 255.255.255.0，网关和 DNS 留空。

在 COM12 终端中设置开发板地址：

```sh
ifconfig eth0 192.168.10.2 netmask 255.255.255.0 up
mkdir -p /root/linux-study/01_hello
```

这个板端地址设置是临时的，重启后需要重新设置。

在 WSL-Ubuntu 中测试：

```sh
ping -c 3 192.168.10.2
```

已验证结果：3 个数据包全部收到，丢包率 0%。

## 4. 从 WSL-Ubuntu 传输程序

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/01_hello
scp -O hello root@192.168.10.2:/root/linux-study/01_hello/
```

`-O` 是大写字母 O，使用 SCP 协议。首次连接需要确认主机身份。本次已成功传输完整文件。

## 5. 在 COM12 终端运行

```sh
cd /root/linux-study/01_hello
chmod +x hello
./hello
echo $?
```

实际输出：

```text
Hello, Linux on i.MX6ULL!
0
```

`./hello` 运行当前目录中的程序；`echo $?` 查看上一条命令的退出状态。`main` 返回 0，表示正常退出。

## 恢复电脑有线网络

有线网口接回路由器上网时，在 Windows 运行 `ncpa.cpl`，打开“以太网”的 IPv4 属性，将 IP 和 DNS 都设为自动获取。拔掉网线不会自动恢复固定地址设置。

## 参考

- SCP 参数：https://man.openbsd.org/scp
- 本次 SSH 的抗量子密钥交换提示：https://www.openssh.org/pq.html
