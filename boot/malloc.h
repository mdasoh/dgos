#pragma once

#include "types.h"

__use_result __malloc __leaf
void *malloc(size_t bytes);

__use_result __malloc __leaf
void *calloc(unsigned num, unsigned size);

void free(void *p);

__use_result __malloc __alloc_align(1) __leaf
void *malloc_aligned(size_t bytes, size_t alignment);

__use_result __malloc __leaf
void *realloc(void *p, size_t bytes);

__use_result __malloc __alloc_align(1)
void *realloc_aligned(void *p, size_t bytes, size_t alignment);

bool malloc_validate();
bool malloc_validate_or_panic();

void malloc_init(void *st, void *en);

void malloc_get_heap_range(void **st, void **en);

#ifndef NDEBUG
void test_malloc();
#endif

void *operator new(size_t size) noexcept;
void *operator new(size_t size, void *p) noexcept;
void *operator new[](size_t size) noexcept;
void operator delete(void *block, unsigned long size) noexcept;
void operator delete(void *block) noexcept;
void operator delete[](void *block) noexcept;
void operator delete[](void *block, unsigned int) noexcept;
