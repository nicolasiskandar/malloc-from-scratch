#define _DEFAULT_SOURCE
#include <pthread.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern void memzero(void *ptr, size_t n);

#ifndef ALIGNMENT
#define ALIGNMENT (sizeof(void *))
#endif

#define ALIGN_UP(x) (((x) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

#define MIN_SPLIT_SIZE (sizeof(block_header_t) + ALIGN_UP(1))

typedef struct block_header
{
    size_t size;
    struct block_header *prev;
    struct block_header *next;
    int free;
} block_header_t;

static block_header_t *free_list_head = NULL;
static pthread_mutex_t global_malloc_lock = PTHREAD_MUTEX_INITIALIZER;

static inline void *header_to_ptr(block_header_t *h)
{
    return (void *)((char *)h + sizeof(block_header_t));
}

static inline block_header_t *ptr_to_header(void *p)
{
    return (block_header_t *)((char *)p - sizeof(block_header_t));
}

static block_header_t *request_space_from_os(size_t payoad_size)
{
    size_t total = sizeof(block_header_t) + payoad_size;
    void *brk = sbrk(total);
    if (brk == (void *)-1) return NULL;

    block_header_t *h = (block_header_t *)brk;
    h->size = payoad_size;
    h->free = 0;
    h->prev = h->next = NULL;
    return h;
}

static void free_list_insert(block_header_t *b)
{
    b->free = 1;
    b->next = free_list_head;
    b->prev = NULL;
    if (free_list_head) free_list_head->prev = b;
    free_list_head = b;
}

static void free_list_remove(block_header_t *b)
{
    if (b->prev) b->prev->next = b->next;
    else free_list_head = b->next;
    if (b->next) b->next->prev = b->prev;
    b->next = b->prev = NULL;
    b->free = 0;
}

static void try_split_block(block_header_t *b, size_t size)
{
    if (b->size < size + sizeof(block_header_t) + ALIGN_UP(1))
        return;

    char *user_ptr = (char *)header_to_ptr(b);
    block_header_t *new_block = (block_header_t *)(user_ptr + size);
    new_block->size = b->size - size - sizeof(block_header_t);
    new_block->free = 1;
    b->size = size;

    new_block->prev = b->prev;
    new_block->next = b->next;
    if (new_block->prev) new_block->prev->next = new_block;
    else free_list_head = new_block;
    if (new_block->next) new_block->next->prev = new_block;
}

static void coalesce(block_header_t *b)
{
    char *b_end = (char *)header_to_ptr(b) + b->size;
    for (block_header_t *cur = free_list_head; cur; cur = cur->next)
    {
        if (cur == b) continue;
        if ((char *)cur == b_end)
        {
            free_list_remove(cur);
            b->size += sizeof(block_header_t) + cur->size;
            break;
        }
    }
}

static block_header_t *find_free_block(size_t size)
{
    for (block_header_t *cur = free_list_head; cur; cur = cur->next)
        if (cur->size >= size)
            return cur;
    return NULL;
}

void *malloc(size_t size)
{
    if (size == 0)
        return NULL;
    size = ALIGN_UP(size);
    pthread_mutex_lock(&global_malloc_lock);

    block_header_t *b = find_free_block(size);
    if (b)
    {
        free_list_remove(b);
        try_split_block(b, size);
        pthread_mutex_unlock(&global_malloc_lock);
        return header_to_ptr(b);
    }

    block_header_t *newb = request_space_from_os(size);
    if (!newb)
    {
        pthread_mutex_unlock(&global_malloc_lock);
        return NULL;
    }

    pthread_mutex_unlock(&global_malloc_lock);
    return header_to_ptr(newb);
}

void free(void *ptr)
{
    if (!ptr) return;
    pthread_mutex_lock(&global_malloc_lock);

    block_header_t *h = ptr_to_header(ptr);
    free_list_insert(h);
    coalesce(h);
    pthread_mutex_unlock(&global_malloc_lock);
}

void *calloc(size_t nmemb, size_t size)
{
    size_t total;
    if (__builtin_mul_overflow(nmemb, size, &total))
        return NULL;

    void *p = malloc(total);
    if (p) memzero(p, total);
    return p;
}

void *realloc(void *ptr, size_t size)
{
    if (ptr == NULL) return malloc(size);
    if (size == 0)
    {
        free(ptr);
        return NULL;
    }

    block_header_t *h = ptr_to_header(ptr);
    size = ALIGN_UP(size);

    if (h->size >= size)
    {
        pthread_mutex_lock(&global_malloc_lock);
        try_split_block(h, size);
        pthread_mutex_unlock(&global_malloc_lock);
        return ptr;
    }

    void *newptr = malloc(size);
    if (!newptr) return NULL;
    memcpy(newptr, ptr, h->size);
    free(ptr);
    return newptr;
}
