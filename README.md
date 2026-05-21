# ReactorHttpKit

Current focus: build the Reactor core first.

Read these files now:

- `include/net/InetAddress.h`
- `include/net/Socket.h`
- `include/net/Channel.h`
- `include/net/Epoll.h`
- `include/net/EventLoop.h`
- `examples/event_loop_demo.cpp`

Current verified path:

```text
timerfd -> Channel -> Epoll -> EventLoop -> callback
```

Run:

```bash
cmake -S . -B build
cmake --build build
./build/examples/event_loop_demo
```
