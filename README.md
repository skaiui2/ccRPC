# ccRPC

A Remote Procedure Call Protocol.

## USE

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

