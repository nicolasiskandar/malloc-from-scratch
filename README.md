# malloc from scratch

Custom memory allocator implemented in C and some x86-64 assembly, supporting malloc, free, calloc, and realloc.

## Highlights
- **Allocator core:** A simple `malloc`/`free`/`calloc`/`realloc` stack built on top of `sbrk`, featuring an intrusive doubly-linked free list, block splitting, and coalescing to limit fragmentation.
- **Thread safety:** Global allocator lock (`pthread_mutex_t`) ensures correctness when multiple worker threads allocate or free in parallel; critical for real-world quant infrastructure.
- **Lean primitives:** Hand-written `memzero` in x86_64 assembler keeps zeroing payloads ultra-miniature while avoiding libc dependencies.
- **Instrumentation-ready:** `main.c` exercises allocations of varying sizes, frees, and a `realloc` path so experiments or benchmarking harnesses can be added quickly.

## Build
```sh
gcc -Wall -pthread main.c malloc.c memzero.s -o main
```
`-pthread` pulls in the POSIX thread library needed for the allocator lock and `sbrk` guard.

## Run / Example Output
```sh
./main
allocated a=0x1234... b=0x1250... c=0x1278...
freed b
allocated d=0x1250...
realloc result: hello
```
