# tlog: 基于广播，转发输入输出到其他终端

**仅限linux平台**

---

另外有一个基于linux信号端口通知方式，见signal分支。

---

之前遇到过一个小需求，多线程下的输入输出不加同步会交错在一起无法阅读，即使加上同步，也很难区分哪一条输出是哪一个线程的。

所以打算采用socket通信方式，将输入输出进行转发。

---

# 使用方法

在你的程序中

```c++
#include <tlog/tlog.hpp>
tlog::tbuf buf("name");
std::ostream tout(&buf);
std::ostream tin(&buf);
tin >> var;	    // 同 std::cin
tout << var;	// 同 std::cout
```

在另一个终端

```shell
tlog_attach	<name>				# 通过name连接
```

PS：
1. 如果出现了同名，`tlog_attach`会随机连接到其中某一个。
2. 如果多个`tlog_attach`尝试同时连接同一个，则只有一个能连接成功。

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

tlog提供一个可执行程序tlog_attach，该程序从命令行获取目标的name，并在本地打开一个端口进行监听，同时向28888发送UDP广播，广播数据格式为`<name> <port> <ip>`。其中`<name>`当前希望建立连接的名称，而后两项`<port> <ip>`则表明自己的地址，用于接受连接。（目前`<ip>`没有发挥作用，默认在`127.0.0.1`上尝试连接，即无法跨主机发送）

`tlog::buf`在构造时会`fork`一个子进程，并通过`socketpair`和子进程进行双向通信。而子进程则会监听28888端口上的UDP广播，当收到广播数据，并确认name匹配后，则会尝试连接。

---

# TODO

1. 部分情况下，转发子进程异常退出（如人为发送SIGKILL信号），会导致主进程的崩溃，主要是由于`socketpair`断开后，主进程会收到SIGPIPE信号而退出。

- [ ] 尝试修复该问题，确保不会影响主进程的稳定性。

2. 完善udp广播报文的发送，使得`tlog_attach`可以发送自己的ip地址，以跨设备连接。

- [ ] 局域网跨设备通信。
