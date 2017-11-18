#pragma once
#include "types.h"
#include "spinlock.h"
#include "vector.h"
#include "bit_alloc.h"

class ioapic_t {
public:
    static ioapic_t *alloc(size_t count);

    bool init(uint8_t id, uint32_t addr, uint8_t irq_base);
    ~ioapic_t();

    uint8_t irq_count() const;

    static size_t count();


    // Returns 0 on failure
    // Pass 0 to allocate 1 vector, 1 to allocate 2 vectors,
    // 4 to allocate 16 vectors, etc.
    // Returns a vector with log2n LSB bits clear.
    static uint8_t aligned_vectors(uint8_t log2n);


private:
    uint32_t read(uint32_t reg);
    void write(uint32_t reg, uint32_t val);
    void chgbits(uint32_t reg, uint32_t clr, uint32_t set);

    void set_unmask(uint8_t intin, bool unmask);

    uint32_t volatile *ptr;
    uint32_t addr;
    uint8_t irq_base;
    uint8_t intr_base;
    uint8_t id;
    uint8_t entries;
    spinlock_t lock;

    static ioapic_t *ioapic_list;
    static size_t ioapic_count;
    static bit_alloc_t<256> irq_allocator;
    static spinlock_t irq_allocator_lock;
};
