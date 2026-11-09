---
paths:
  - "src/**"
  - "include/fiaisdk/**"
---
# Concurrency & async

- Until an async ADR exists, do not introduce threads, executors or callbacks — ask first.
- Long operations must not block the caller's UI thread: expose async C API as
  `fiaisdk_x_async(handle, args, callback, user_data, out_request_id)`.
- Document for every callback: which thread runs it, whether it can be called after
  `destroy`/`cancel` (it must not), and that `user_data` is opaque and caller-owned.
- Threads are owned by core (one executor) — modules do not spawn `std::thread`.
- Shared mutable state: guard with `std::mutex`; never call user callbacks while holding a lock.
- Handle destroy must join/cancel outstanding work deterministically.
- Qt: SDK never touches QObject; the Qt adapter marshals callbacks with `QMetaObject::invokeMethod`.
