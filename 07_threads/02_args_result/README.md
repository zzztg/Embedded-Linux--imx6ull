# 实验 07-02：线程参数、返回指针与对象生命周期

状态：学习者已上板，输出符合预期：join rc=0，job.sum 与返回指针解引用均为42，返回指针指向原成员（yes）。首次误用echo $/后已解释退出状态规则；学习者随后贴出echo $?为0并明确确认无误，按学习者确认记录程序退出0（未贴出完整重跑序列）。学习者正确解释：job是main的局部对象，工作线程返回时main未结束，对象生命周期仍有效。尚未收到学习者编译日志。下一节进入数据竞争与互斥锁。上节 07-01 已上板成功，学习者正确区分通过指针修改原对象与复制数值后修改局部对象；已补充局部对象生命周期结束前就与原对象独立的解释。本节不增加新的 pthread 函数，仅展开 pthread_join 的第二个参数。

教师隔离验证：ARM 静态程序通过 -Wall -Wextra -Werror -pthread 编译链接，file 确认为 ARM 32 位静态程序；WSL 本机版本使用同样警告与线程选项编译后运行，业务结果为 42，返回指针与原成员地址相等，退出 0。学习目录的正式 build 尚未由教师生成，留给学习者亲手编译。

## 本节目标

1. pthread_create 只传一个 void * 参数，可以用结构体组织多个业务参数。
2. pthread_join 的 int 返回值表示等待操作是否成功；通过第二个参数接收线程入口返回的 void *，两者含义不同。
3. 传递和返回的是地址值，不自动复制整个结构体，也不延长对象生命周期。
4. 指针变量的生命周期与它所指对象的生命周期必须分别判断。

## 结构体参数

```c
struct job {
    int left;
    int right;
    int sum;
};
```

这是类型定义，不会自动创建一个 job 对象。main 中的 `struct job job = { .left = 12, .right = 30, .sum = 0 };` 创建对象，并按成员名初始化。类型名和对象变量名可以相同；这是 C 结构体标签与变量的不同名字空间，本例采用这种常见写法。

创建线程时传入 `&job`；worker 中 `struct job *job = arg;` 建立一个指向 main 中该结构体的局部指针。`job->sum` 等价于 `(*job).sum`；main 中有结构体对象，所以使用 `job.sum`。两处都在访问同一个成员对象。

本例无动态内存分配，不涉及 malloc/free。main 在创建前打印初值，随后等待，join 成功后才读取计算结果，避免了未同步的并发读写。

## 线程返回的是什么

```c
return &job->sum;
```

线程入口返回类型是 void *，这里返回的是 main 中结构体的 sum 成员地址。它不是返回整数 42，也不是返回工作线程局部指针 job 自身的地址。工作线程结束时其局部指针不再存活，但 main 中的结构体仍然存活。

## pthread_join 的两个输出

接口原型：

```c
int pthread_join(pthread_t thread, void **retval);
```

用法：

```c
void *result = NULL;
int rc = pthread_join(worker_thread, &result);
```

- `rc`：join 操作是否成功，0 表示成功，非零为错误编号。
- `result`：join 成功后，存放工作线程入口返回的那个指针值。
- `&result`：让 join 能修改 result 变量。result 的类型为 void *，因此它的地址类型是 void **。

把 void * 看成一个完整的指针类型：`&result` 就是“这个指针变量的地址”。二级指针并不意味着必须连续解引用两次访问整数。先让 join 写好 result，再在 C 中赋给 int *，最后解引用一次即可：

```c
int *sum = result;
printf("%d\n", *sum);
```

不要改为 `int result`，也不要用 `(void **)&some_int_pointer` 绕开类型要求。按 API 使用真正的 void * 接收变量，再转换为正确的对象指针。

最后一行 `sum == &job.sum ? "yes" : "no"` 使用 C 条件运算符；地址相等打印 yes。验证返回指针指向的就是主线程中原来的成员。

## 生命周期：返回地址是否有效

本例：main 创建结构体 → 创建线程 → 工作线程修改成员并返回成员地址 → main join 成功 → main 读取结果 → main 返回。整个使用期间结构体都有效。

下面是错误示意，**不加入程序、不运行**：

```c
static void *bad_worker(void *arg)
{
    int local_sum = 42;
    return &local_sum;
}
```

local_sum 是工作线程入口自己的自动局部对象，入口返回后它的生命周期结束。join 只能传回地址值、等待线程结束，无法让这个整数继续存活。解引用该地址会产生未定义行为，不能依靠某次碰巧打印出 42 来证明正确。

同理，把普通辅助函数中局部对象的地址交给新线程后立即从辅助函数返回，也不能保证线程使用该对象时对象仍然存活。共享地址空间不等于对象永久有效。

## WSL：亲手编译

逐条执行：

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/07_threads/02_args_result
cat src/thread_result.c
make -n
make
echo $?
file build/thread_result
```

预期编译退出 0，ARM 32 位静态可执行文件，编译与链接两步都有 -pthread。

## COM12：创建目标目录

```sh
mkdir -p /root/linux-study/07_threads/02_args_result
```

## WSL：传输

在本节 WSL 目录中执行：

```sh
scp -O build/thread_result root@192.168.10.2:/root/linux-study/07_threads/02_args_result/
```

若开发板重启导致静态 IP 丢失，先在 COM12 查看 ifconfig eth0，必要时恢复 192.168.10.2/24，再传输。

## COM12：运行

```sh
cd /root/linux-study/07_threads/02_args_result
chmod +x thread_result
./thread_result
echo $?
```

预期输出：

```text
main: before create sum=0
worker: left=12, right=30
worker: sum=42; returning its address
main: join rc=0, job.sum=42, *sum=42
main: result points to job.sum: yes
```

退出状态 0。rc=0 表示等待操作成功；42 是业务计算结果；yes 说明返回的是原成员的地址，三者不能混为一个“退出码”。

## 后续

下一步进入共享数据的数据竞争与互斥锁。本节先把参数地址、返回指针和生命周期补齐，避免在同步实验中同时引入这些概念。

参考：[pthread_create](https://man7.org/linux/man-pages/man3/pthread_create.3.html)、[pthread_join](https://man7.org/linux/man-pages/man3/pthread_join.3.html)。
