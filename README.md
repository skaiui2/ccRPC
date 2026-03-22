# ccRPC

[中文介绍](./docs/中文/README中文.md)

A Remote Procedure Call Protocol.

I built it because I needed a reliable way to invoke remote functions in embedded environments, and there simply wasn’t a pure‑C RPC protocol that was small, portable, and practical enough.

ccRPC makes calling a remote function feel just like calling a local one, and it provides the following features:

1. **Pure C implementation**, using a C‑macro–based XDEF DSL for automatic code generation
2. **TLV encoding**, compact and predictable for embedded systems
3. **Synchronous call model**, simple and natural to use
4. **Thread‑safe runtime**, suitable for both MCU and Linux environments
5. **Zero dependencies** — fully self‑contained memory management and data structures, portable to any platform
6. **Extremely small footprint**, only a few kilobytes of RAM required

## DESIGN

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

The nodeB call nodeA's functions:

Run nodeA:

```
cd nodeA
mkdir build
cd build
dd if=/dev/urandom of=big.bin bs=1M count=1024
make
./nodeA
```

Run nodeB:

```
cd nodeB
mkdir build
cd build
```

copy the big.bin，then

```
make
./nodeB
```

like this:

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

