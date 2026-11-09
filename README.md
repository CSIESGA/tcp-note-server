# TCP Note Server

A TCP server written in C that lets multiple clients read and update notes stored on disk. The server uses `poll()` to handle client connections in a single event loop and `fcntl()` byte-range locks to coordinate access to the note files.

## Build and run

From the repository root on Linux with GCC and Make installed:

```sh
make
./server 8080
```

The repository includes a Python public checker and sample test cases.
