#pragma once
#include "types.h"
#include "bitsearch.h"

// Bitmap allocator

template<unsigned size>
class bit_alloc_t {
public:
    bit_alloc_t();

    int alloc(unsigned sz = 1);

    // Allocate even multiple of ceil(log2(sz))
    int alloc_aligned(unsigned log2n);

    bool take(int start, unsigned count = 1);

    void free(int start, unsigned count = 1);

private:
    uint32_t bitmap[(size + 31) >> 5];
};
