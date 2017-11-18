#include "bit_alloc.h"

template<unsigned size>
bit_alloc_t<size>::bit_alloc_t()
    : bitmap({})
{
}

template<unsigned size>
int bit_alloc_t<size>::alloc(unsigned sz)
{

}

template<unsigned size>
int bit_alloc_t<size>::alloc_aligned(unsigned log2n)
{

}

template<unsigned size>
bool bit_alloc_t<size>::take(int start, unsigned count)
{

}

template<unsigned size>
void bit_alloc_t<size>::free(int start, unsigned count)
{

}
