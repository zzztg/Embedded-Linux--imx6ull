# IPC基础章节

2026-11-11调整：保留已完成的管道相关实验，新增08_fifo作为本章收尾。取消后续独立IPC进阶专项；共享内存、消息队列、进程间同步等在实际项目用到时再补充。

| 小节 | 内容 |
|---|---|
| 01_pipe | 匿名管道、关闭端点、EOF |
| 02_pipe_exec | 捕获子程序输出 |
| 03_nonblock_pipe | 非阻塞读取与EAGAIN |
| 04_poll | 就绪、超时与挂断 |
| 05_poll_streams | 同时捕获stdout和stderr |
| 06_sigpipe | SIGPIPE/EPIPE的基础处理 |
| 07_pipe_backpressure | 容量、背压与发送进度 |
| [08_fifo](08_fifo/README.md) | 命名管道、两个独立进程通信与节点生命周期 |

06-08 FIFO两轮板端传输已经完成，并确认阻塞open与read EOF的区别；当前进入08_network/01_tcp_echo。既有章节的理论说明继续保留；早期提到的后续专题安排以当前路线为准。
