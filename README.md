# tlog: 基于信号，转发输入输出到其他终端

**仅限linux平台**

---

之前遇到过一个小需求，多线程下的输入输出不加同步会交错在一起无法阅读，即使加上同步，也很难区分哪一条输出是哪一个线程的。

所以打算采用socket通信方式，将输入输出进行转发。

---

# 使用方法

在你的程序中

```c++
#include <tlog/tlog.hpp>

tlog::tin >> var;	// 同 std::cin
tlog::tout << var;	// 同 std::cout
```

在另一个终端

```shell
ps -ax | grep <your prog name>	# 查找进程pid
tlog_attach	<pid>				# 通过pid连接
```

PS：tlog使用了fork创建子进程，用于转发io（详细实现参考**实现方式**部分）。所以你可能看到**多个进程pid**，如果你的程序没有另外创建子进程，那么**较大的pid**才是用于转发io的进程。而如果你的程序另外创建了子进程，那么想要区分出其中用于转发io的进程并不容易。这将是TODO的一部分。

---

# 编译安装

```shell
mkdir build
cd build
cmake ..
make -j$(proc)
sudo make install
```

---

# 实现方式

`tlog::tin`和`tlog::tout`采用`thread_local`变量，以保证不同线程间没有冲突。

`tlog::tin`和`tlog::tout`绑定到同一个`tlog::buf`上，这同样也是一个`thread_local`变量。

`tlog::buf`在构造时会`fork`一个子进程，并通过`socketpair`和子进程进行双向通信。

而子进程则等待一个带参数`SIGUSR1`信号，用于获取服务端的端口号；当子进程得到端口号后，会尝试进行连接，如果连接成功则开始转发io数据。

tlog提供一个可执行程序tlog_attach，该程序从命令行获取目标进程的pid（通常可以使用ps命令得到），并在本地打开一个端口进行监听，同时向pid进程发送带参数`SIGUSR1`信号，告知其当前的端口号（由于端口号可能冲突，尤其同时运行多个tlog_attach程序时，所以设置固定端口号是不靠谱的，tlog采用信号的方式来告知动态的端口号）。如果pid指定正确，应该可以接收到来自io转发子进程的连接。

---

# TODO

1. 部分情况下，转发子进程异常退出（如人为发送SIGKILL信号），会导致主进程的崩溃，主要是由于`socketpair`断开后，主进程会收到SIGPIPE信号而退出。

- [ ] 尝试修复该问题，确保不会影响主进程的稳定性。
