# malloc from scratch

Custom memory allocator implemented in C and some x86-64 assembly. Supports `malloc`, `free`, `calloc`, and `realloc`.

## Features

- **Allocator core:** First-fit `malloc`/`free`/`calloc`/`realloc` stack backed by an intrusive doubly-linked free list, with block splitting and coalescing to limit fragmentation.
- **Thread safety:** A global allocator lock (`pthread_mutex_t`) protects all allocations and frees, so the allocator is safe to call from multiple threads.
- **Lean primitives:** A hand-written `memzero` in x86-64 assembly zeroes payloads without pulling in libc.
- **Overflow-safe `calloc`:** Uses `__builtin_mul_overflow` to reject `nmemb * size` overflow.
- **Instrumentation-ready:** `main.c` exercises allocations of varying sizes, frees, and a `realloc` path so experiments or benchmarking harnesses can be added quickly.

## How it works

### Heap layout

Every allocation is preceded by an intrusive block header:

```c
typedef struct block_header {
    size_t size;                  /* payload size, not including header */
    struct block_header *prev;    /* free-list links when free */
    struct block_header *next;
    int free;
} block_header_t;
```

The heap grows via `sbrk`. Freed blocks live in a doubly-linked free list headed by `free_list_head`.

### Allocation path

1. `malloc` aligns the requested size up to 8 bytes.
2. `find_free_block` scans the free list for the first block large enough (first-fit).
3. If found, the block is removed from the free list and `try_split_block` carves off any surplus into a new free block.
4. Otherwise `request_space_from_os` extends the heap with `sbrk`.

### Free path

1. `free` re-inserts the block into the free list.
2. `coalesce` merges it with the immediately following free block (by address), shrinking fragmentation.

### Files

| File       | Role                                              |
|------------|---------------------------------------------------|
| `malloc.c` | Allocator: header, free list, malloc/free/calloc/realloc |
| `memzero.s`| Hand-written x86-64 `rep stosb` zeroing routine   |
| `main.c`   | Small demo program exercising the API             |

## API

| Function                              | Notes                                                     |
|---------------------------------------|-----------------------------------------------------------|
| `void *malloc(size_t size)`           | Allocates `size` bytes, aligned to 8. Returns `NULL` for `size == 0`. |
| `void free(void *ptr)`                | Frees a block from `malloc`/`calloc`/`realloc`. No-op for `NULL`. |
| `void *calloc(size_t nmemb, size_t n)`| Zero-initialized; overflow-checked via `memzero`.         |
| `void *realloc(void *ptr, size_t sz)` | `NULL` ptr behaves like `malloc`; `size == 0` frees.      |
| `void memzero(void *ptr, size_t n)`   | Assembly loop zeroing `n` bytes.                          |

## Build

```sh
gcc -Wall -pthread main.c malloc.c memzero.s -o main
```

`-pthread` pulls in the POSIX thread library needed for the allocator lock.

## Run / Example Output

```sh
./main
allocated a=0x1234... b=0x1250... c=0x1278...
freed b
allocated d=0x1250...
realloc result: hello
```
