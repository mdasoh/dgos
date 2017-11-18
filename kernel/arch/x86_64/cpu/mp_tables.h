#pragma once
#include "types.h"
#include "spinlock.h"

#define MP_TABLE_TYPE_CPU       0
#define MP_TABLE_TYPE_BUS       1
#define MP_TABLE_TYPE_IOAPIC    2
#define MP_TABLE_TYPE_IOINTR    3
#define MP_TABLE_TYPE_LINTR     4
#define MP_TABLE_TYPE_ADDRMAP   128
#define MP_TABLE_TYPE_BUSHIER   129
#define MP_TABLE_TYPE_BUSCOMPAT 130

//
// MP Tables

struct mp_table_hdr_t {
    char sig[4];
    uint32_t phys_addr;
    uint8_t length;
    uint8_t spec;
    uint8_t checksum;
    uint8_t features[5];
};

struct mp_cfg_tbl_hdr_t {
    char sig[4];
    uint16_t base_tbl_len;
    uint8_t spec_rev;
    uint8_t checksum;
    char oem_id_str[8];
    char prod_id_str[12];
    uint32_t oem_table_ptr;
    uint16_t oem_table_size;
    uint16_t entry_count;
    uint32_t apic_addr;
    uint16_t ext_tbl_len;
    uint8_t ext_tbl_checksum;
    uint8_t reserved;
};

// entry_type 0
struct mp_cfg_cpu_t {
    uint8_t entry_type;
    uint8_t apic_id;
    uint8_t apic_ver;
    uint8_t flags;
    uint32_t signature;
    uint32_t features;
    uint32_t reserved1;
    uint32_t reserved2;
};

#define MP_CPU_FLAGS_ENABLED_BIT    0
#define MP_CPU_FLAGS_BSP_BIT        1

#define MP_CPU_FLAGS_ENABLED        (1<<MP_CPU_FLAGS_ENABLED_BIT)
#define MP_CPU_FLAGS_BSP            (1<<MP_CPU_FLAGS_BSP_BIT)

// entry_type 1
struct mp_cfg_bus_t {
    uint8_t entry_type;
    uint8_t bus_id;
    char type[6];
};

// entry_type 2
struct mp_cfg_ioapic_t {
    uint8_t entry_type;
    uint8_t id;
    uint8_t ver;
    uint8_t flags;
    uint32_t addr;
};

#define MP_IOAPIC_FLAGS_ENABLED_BIT   0

#define MP_IOAPIC_FLAGS_ENABLED       (1<<MP_IOAPIC_FLAGS_ENABLED_BIT)

// entry_type 3 and 4 flags

#define MP_INTR_FLAGS_POLARITY_BIT    0
#define MP_INTR_FLAGS_POLARITY_BITS   2
#define MP_INTR_FLAGS_TRIGGER_BIT     2
#define MP_INTR_FLAGS_TRIGGER_BITS    2

#define MP_INTR_FLAGS_TRIGGER_MASK \
        ((1<<MP_INTR_FLAGS_TRIGGER_BITS)-1)

#define MP_INTR_FLAGS_POLARITY_MASK   \
        ((1<<MP_INTR_FLAGS_POLARITY_BITS)-1)

#define MP_INTR_FLAGS_TRIGGER \
        (MP_INTR_FLAGS_TRIGGER_MASK<<MP_INTR_FLAGS_TRIGGER_BITS)

#define MP_INTR_FLAGS_POLARITY \
        (MP_INTR_FLAGS_POLARITY_MASK << \
        MP_INTR_FLAGS_POLARITY_BITS)

#define MP_INTR_FLAGS_POLARITY_DEFAULT    0
#define MP_INTR_FLAGS_POLARITY_ACTIVEHI   1
#define MP_INTR_FLAGS_POLARITY_ACTIVELO   3

#define MP_INTR_FLAGS_TRIGGER_DEFAULT     0
#define MP_INTR_FLAGS_TRIGGER_EDGE        1
#define MP_INTR_FLAGS_TRIGGER_LEVEL       3

#define MP_INTR_FLAGS_POLARITY_n(n)   ((n)<<MP_INTR_FLAGS_POLARITY_BIT)
#define MP_INTR_FLAGS_TRIGGER_n(n)    ((n)<<MP_INTR_FLAGS_TRIGGER_BIT)

#define MP_INTR_TYPE_APIC   0
#define MP_INTR_TYPE_NMI    1
#define MP_INTR_TYPE_SMI    2
#define MP_INTR_TYPE_EXTINT 3

extern char const * const intr_type_text[];
extern size_t intr_type_text_count;

// entry_type 3
struct mp_cfg_iointr_t {
    uint8_t entry_type;
    uint8_t type;
    uint16_t flags;
    uint8_t source_bus;
    uint8_t source_bus_irq;
    uint8_t dest_ioapic_id;
    uint8_t dest_ioapic_intin;
};

// entry_type 4
struct mp_cfg_lintr_t {
    uint8_t entry_type;
    uint8_t type;
    uint16_t flags;
    uint8_t source_bus;
    uint8_t source_bus_irq;
    uint8_t dest_lapic_id;
    uint8_t dest_lapic_lintin;
};

// entry_type 128
struct mp_cfg_addrmap_t {
    uint8_t entry_type;
    uint8_t len;
    uint8_t bus_id;
    uint8_t addr_type;
    uint32_t addr_lo;
    uint32_t addr_hi;
    uint32_t len_lo;
    uint32_t len_hi;
};

// entry_type 129
struct mp_cfg_bushier_t {
    uint8_t entry_type;
    uint8_t len;
    uint8_t bus_id;
    uint8_t info;
    uint8_t parent_bus;
    uint8_t reserved[3];
};

// entry_type 130
struct mp_cfg_buscompat_t {
    uint8_t entry_type;
    uint8_t len;
    uint8_t bus_id;
    uint8_t bus_mod;
    uint32_t predef_range_list;
};

struct mp_bus_irq_mapping_t {
    uint8_t bus;
    uint8_t intr_type;
    uint8_t flags;
    uint8_t device;
    uint8_t irq;
    uint8_t ioapic_id;
    uint8_t intin;
};

struct mp_ioapic_t {
    uint8_t id;
    uint8_t base_intr;
    uint8_t vector_count;
    uint8_t irq_base;
    uint32_t addr;
    uint32_t volatile *ptr;
    spinlock_t lock;
};
