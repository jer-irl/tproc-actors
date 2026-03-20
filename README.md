# tproc-actors

Actor model framework built on [threadprocs](https://github.com/jer-irl/threadprocs).

This README is handwritten, the code for this PoC is a bit sloppy (in all regards).

## Demo

A threadproc server is started, and then two actors are launched into its address space.

The requester passes multiple `std::map<...::time_point, std::function<...>>` objects to the service, which spins and notifies the requester when timers expire.
The in-memory data structures are accessed from entirely distinct executables.

https://github.com/user-attachments/assets/e037d7c1-96bb-4425-9a18-ddf67744f0d1

## What differentiates this from others?

This is built on [threadprocs](https://github.com/jer-irl/threadprocs), which provides a single shared memory address space for sub(thread)processes, which may be launched independently.
These threadprocs can even be separately-compiled binaries.

The shared address-space means that complicated data structures (nested) can be passed by owning reference between actors, even if those actors come from different threadprocs and different binaries.

## Is this a good idea?

"Good" is in the eye of the beholder.

That said, probably not.
Conventionally, one would use plain ol' pthreads in a single multi-threaded process.
If the multi-process model was important, the conventional approach would be to use explicit shared memory regions, and flat data (or carefully allocated data) resident in that shared memory.
Notably, idiomatic pointer-based C++ data structures don't work in the shared memory region approach, but _do_ work in a shared address space (multi-threading, or threadprocs).

## What hacks are needed?

Each threadproc has its own global runtime state, including its own allocators, etc.
Though a threadproc can follow pointers to memory allocated by another threadproc, it cannot free that memory.

Messages between actors in this framework use customized `unique_ptr`-like pointers which remember where they were allocated.
When it comes time to destroy the object, the data pointer enqueued in a global per-threadproc ring.
The `AgentRegistry` class polls this queue and frees memory in the threadproc that allocated it.

## How brittle is this?

Very.

## How's the code?

Proof-of-concept __only__:

- uses mutexes
- unnecessaryallocations
- too much metaprogramming
- slow data structures
- unsafe `reinterpret_cast`s
- `std::type_info` equality checks between instances from different threadprocs
