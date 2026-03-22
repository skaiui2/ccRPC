# ccRPC

ccRPC是一个远程调用方法。

我写它的目的是因为我需要在嵌入式环境中远程调用方法，但是没什么纯c的好用的rpc协议。

他可以让本地调用远程函数非常自然，有以下特征：

1.纯c，使用c宏配置xdef实现自动代码生成

2.tlv编码

3.同步调用

4.多线程安全

5.零依赖，可移植到任何平台，内存管理、数据结构全是自己实现的

6.内存占用极小，几KB就行



## DESIGN

ccRPC是从完整的三层协议CSC中单独拆分下来的，ccRPC是第三层：

```
           application
             │
             ▼
           ccRPC
   (IDL, TLV, Pending, Transport)
             │
             ▼
         Transport 
   ┌─────────┴───────────────────────┐
   │                                 │
   ▼                                 ▼
 TCP Transport                   SCP Transport
 (use TCP or)        ┌─────────────────────────────────┐
                     │  SCP (Stream Control Protocol)  │
                     └─────────────────────────────────┘
                        │            │            │
                        ▼            ▼            ▼
                  Network Provider Network Provider Network Provider
                       = IP           = CC         = None/Set by yourself
                     (UDP/IP)    (SPI/UART/CAN)  
                        │            │
                        ▼            ▼
                    UDP Socket     Raw Link (SPI/UART/…)

```

## run

运行A节点：

```
cd nodeA
mkdir build
cd build
dd if=/dev/urandom of=big.bin bs=1M count=1024
make
./nodeA
```

运行B节点：

```
cd nodeB
mkdir build
cd build

```

记得复制粘贴在nodeA的build目录下的大文件，因为我们的测试是一方调用另一方读取文件随机内容，然后自己读取一遍，最后对双方内容进行校验，然后，另一方调用对方的shell命令，也进行校验：

```
make
./nodeB
```

最后运行：

```
skaiuijing@ubuntu:~/rpc/nodeA/build$ ./nodeA 
FS PASS iter=85001 off=647459447 size=540 local=59B1843C remote=59B1843C
FS PASS iter=86001 off=275305091 size=352 local=1AA2FB65 remote=1AA2FB65
FS PASS iter=87001 off=910748057 size=117 local=D0375530 remote=D0375530
FS PASS iter=88001 off=509610752 size=30 local=5031646D remote=5031646D
FS PASS iter=89001 off=631070769 size=484 local=EC9CCB57 remote=EC9CCB57
FS PASS iter=90001 off=506470502 size=47 local=F1B572B8 remote=F1B572B8
FS PASS iter=91001 off=54008201 size=258 local=0EBB5802 remote=0EBB5802
FS PASS iter=92001 off=287570502 size=411 local=C829267D remote=C829267D
FS PASS iter=93001 off=672000202 size=557 local=8114031D remote=8114031D
FS PASS iter=94001 off=544743775 size=539 local=868DB5EC remote=868DB5EC
FS PASS iter=95001 off=716416219 size=897 local=4CE9AEFE remote=4CE9AEFE
FS PASS iter=96001 off=980444305 size=853 local=5C2F33FC remote=5C2F33FC
FS PASS iter=97001 off=548984588 size=898 local=2D64E2D0 remote=2D64E2D0
FS PASS iter=98001 off=259275975 size=388 local=3837FA47 remote=3837FA47
FS PASS iter=99001 off=55860540 size=471 local=C1148A87 remote=C1148A87
FS PASS iter=100001 off=218966133 size=689 local=BA76A615 remote=BA76A615
FS PASS iter=101001 off=947927471 size=33 local=626C154B remote=626C154B
```

nodeB:

```
skaiuijing@ubuntu:~/rpc/nodeB/build$ ./nodeB 
SHELL PASS iter=116001 cmd=whoami local=2DDBD0C1 remote=2DDBD0C1
SHELL PASS iter=117001 cmd=echo hello local=363A3020 remote=363A3020
SHELL PASS iter=118001 cmd=whoami local=2DDBD0C1 remote=2DDBD0C1
SHELL PASS iter=119001 cmd=echo hello local=363A3020 remote=363A3020
SHELL PASS iter=120001 cmd=whoami local=2DDBD0C1 remote=2DDBD0C1
SHELL PASS iter=121001 cmd=whoami local=2DDBD0C1 remote=2DDBD0C1
SHELL PASS iter=122001 cmd=whoami local=2DDBD0C1 remote=2DDBD0C1
SHELL PASS iter=123001 cmd=id local=47236629 remote=47236629
SHELL PASS iter=124001 cmd=id local=47236629 remote=47236629
SHELL PASS iter=125001 cmd=echo hello local=363A3020 remote=363A3020
SHELL PASS iter=126001 cmd=echo hello local=363A3020 remote=363A3020
SHELL PASS iter=127001 cmd=echo hello local=363A3020 remote=363A3020
SHELL PASS iter=128001 cmd=echo hello local=363A3020 remote=363A3020
SHELL PASS iter=129001 cmd=echo hello local=363A3020 remote=363A3020
SHELL PASS iter=130001 cmd=echo hello local=363A3020 remote=363A3020
SHELL PASS iter=131001 cmd=id local=47236629 remote=47236629
SHELL PASS iter=132001 cmd=echo hello local=363A3020 remote=363A3020
SHELL PASS iter=133001 cmd=whoami local=2DDBD0C1 remote=2DDBD0C1
SHELL PASS iter=134001 cmd=id local=47236629 remote=47236629
SHELL PASS iter=135001 cmd=echo hello local=363A3020 remote=363A3020
SHELL PASS iter=136001 cmd=id local=47236629 remote=47236629
```

