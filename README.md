# Flute

A multi-thread TCP network library using C++11.

## Quick Start

We can setup a simple http server based on this TCP library.

- resources are in `./root`
- execuatble is built from `./flute/net/http/tests/HttpServer_test.cc`. Http callbacks are defined here.

```bash
# build the project
sh ./build_flute.sh
# HttpServer is one of the execuatbles
./build/debug_cpp11/bin/HttpServer_test 8 3 0 1
```

_args_

- Num of threads in the sub-reactor poll
- LogLevel. 3 means `ERROR`
- Copy Mode. 0 means `sendflie`(zerocopy)
- Chunk size(KB) of the copier

## Features

- Macro-based multi-thread logging
- FIFO Buffer for byte-oriented IO
- Pooler for IO multiplexing
- Main-sub Reactor, in _one-reactor-per-thread_ manner
- ZeroCopier for efficient file transfer
- HTTP Server

## Components

**Common**

- Buffer
- LogLine
- CurrentThread
- Mutex, Condition
- Thread
- Timestamp
- ZeroCopier

**Network**

- Socket
- Channel
- Pooler
- Reactor
- TimerQueue
- Acceptor
- TCPServer
- HttpServer

## Conventions

Based on [Google C++ Style](https://google.github.io/styleguide/cppguide.html)

- naming
  - Files: `Reactor.h`, `Reactor.cc`
  - variable: `offset`, `tcp_conn_ptr`
  - class: `Reactor`, `ZeroCopier`
  - member variables: `m_socket_fd`, `m_mutex`, `m_is_writing`
  - funtions: `to_write()`, `send_one_chunk()`
  - global variable: `g_conn_counter`
  - enums: `kZeroCopy`
  - macros: `LOG_INFO`
  - namespace: `flute`
- Header: `#define FLUTE_NET_REACTOR`
- using namespace
  - `flute` contains all of the definitions.
  - `std` to access standard library.
  - anonymous to hide details

### RAII

**Resource acquisition is initialization** is a programming idiom used in several object-oriented, statically-typed programming languages to describe a particular language behavior.

In RAII, holding a resource is a class invariant, and is tied to object lifetime.

- scoped variable
  - `MutexGuard lock;` for critial resources
- combined with std::shared_ptr, to automatically call deconstructors and free the resources.
  - `Thread`: pthread id
  - `ZeroCopier`: source file fd
  - `TCPConnection`: socket fd
  - `TimerQueue`: timer fd
  - `ReactorPool`: Reactors

## Performance Investigation

- [Locust](https://github.com/locustio/locust) is recommened for scalable loadtest.
- [FlameGraph](https://github.com/brendangregg/FlameGraph) can help to trace stack and identify the bottleneck of the system.

```bash
sudo ./flamegraph.sh
```

## References

- [muduo](https://github.com/chenshuo/muduo)
- Unix NetWork Programming
- Linux 多线程服务端编程：使用 muduo C++网络库
