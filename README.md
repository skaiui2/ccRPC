# ccRPC

A Remote Procedure Call Protocol.

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



## RPC USE

Write the xdef, like:

``` 
RPC_METHOD_PROVIDER(
    fs_read,
    "fs.read",
    PARAMS(
        FIELD(string, path);
        FIELD(u32, offset);
        FIELD(u32, size);
    ),
    RESULTS(
        FIELD(bytes, data);
        FIELD(u32, len);
        FIELD(u32, eof);
    )
)

RPC_METHOD_REQUEST(
    shell_exec,
    "shell.exec",
    PARAMS(
        FIELD(string, cmd);
    ),
    RESULTS(
        FIELD(string, output);
        FIELD(u32, exitcode);
    )
)
```



## run

The nodeB call nodeA's functions:

Run nodeA:

```
cd nodeA
mkdir build
cd build
make
./nodeA
```

Run nodeB:

```
cd nodeB
mkdir build
cd build
make
./nodeB
```

like this:

```
skaiuijing@ubuntu:~/rpc/nodeA/build$ ./nodeA 
NodeA: fs_read(path=/etc/config, offset=0, size=128)
NodeA: shell.exec => 0
NodeA: output=dummy-output-from-NodeB exit=0
```

nodeB:

```
skaiuijing@ubuntu:~/rpc/nodeB/build$ ./nodeB 
NodeB: shell_exec(cmd=echo hello-from-NodeA)
NodeB: fs.read => 0
NodeB: len=16 eof=1
NodeB: data=hello-from-NodeA
```



## SCP

```
                           SCP Transport
 ┌──────────────────────────────────────────────────────────────────────┐
 │                          Core Reliability                            │
 │  • Reliable Stream Delivery (Ordered, No Dup)                        │
 │  • Out‑of‑Order Buffer & Reassembly                                  │
 │  • ACK Handling & Cumulative Acknowledgment                          │
 ├──────────────────────────────────────────────────────────────────────┤
 │                         Loss Recovery & Timers                       │
 │  • Retransmission (RTO, Exponential Backoff)                         │
 │  • Zero‑Window Recovery (Persist Timer)                              │
 │  • Keepalive / Ping for Liveness                                     │
 ├──────────────────────────────────────────────────────────────────────┤
 │                         Flow & Window Control                        │
 │  • Sliding Window Protocol                                           │
 │  • Receiver‑Advertised Window (rcv_wnd)                              │
 │  • Sender Flight Control (snd_una / snd_nxt)                         │
 ├──────────────────────────────────────────────────────────────────────┤
 │                         Transport Flexibility                        │
 │  • Runs over UDP/IP                                                  │
 │  • Runs over SPI / UART / CAN                                        │
 │  • Supports Custom / Raw Links                                       │
 └──────────────────────────────────────────────────────────────────────┘
```

### USE

The scp can base on UDP.You can use it to replace TCP.

Like this:

```c
static cal_udp_ctx_t udpA;
static struct sockaddr_in peerB;
static struct scp_transport_class scp_udp_trans;

int scp_udp_send(void *user, const void *buf, size_t len)
{
    return cal_udp_send(&udpA, buf, len, &peerB);
}

int scp_udp_recv(void *user, void *buf, size_t maxlen)
{
    return cal_udp_recv(&udpA, buf, maxlen, &peerB);
}

int scp_udp_close(void *user)
{
    cal_udp_close(&udpA);
    return 0;
}


#define DST_FD 10
#define SRC_FD 20
int main(void)
{
    scp_udp_trans.send  = scp_udp_send;
    scp_udp_trans.recv  = scp_udp_recv;
    scp_udp_trans.close = scp_udp_close;
    scp_udp_trans.user  = NULL;

    scp_init(16);
    scp_stream_alloc(&scp_udp_trans, SRC_FD, DST_FD);

    printf("NodeA: SCP stream opened, fd=%d\n", SRC_FD);

    scp_time_init();

    if (cal_udp_open(&udpA, "0.0.0.0", 9001) < 0) {
        perror("NodeA udp_open");
        return -1;
    }

    int flags = fcntl(udpA.sockfd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
    } else {
        if (fcntl(udpA.sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
            perror("fcntl F_SETFL O_NONBLOCK");
        } else {
            printf("udp socket set non-blocking\n");
        }
    }

    peerB.sin_family = AF_INET;
    peerB.sin_port   = htons(9002);
    peerB.sin_addr.s_addr = inet_addr("127.0.0.1");

    uint8_t buf[2048];
    uint8_t appbuf[4096];

    char msg[1800] = "hello from NodeA12";
    for(int i = 20; i < 1800; i++) {
        msg[i] = i;
    }
    uint64_t a = 0;
    while (1)
    {
        scp_send(SRC_FD, msg, sizeof(msg));
        
        int n = scp_udp_trans.recv(NULL, (void *)buf, sizeof(buf));
        if (n > 0) {
            scp_input(SRC_FD, buf, n);
        }

        int rn = scp_recv(SRC_FD, appbuf, sizeof(appbuf));
        if (rn > 0) {
            printf("NodeA recv from SCP: %s, a: %lu\n", appbuf, a);
        }
       
        a++;
        usleep(10000);
    }
    return 0;
}
```

