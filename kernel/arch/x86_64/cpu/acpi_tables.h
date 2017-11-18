#pragma once
#include "types.h"
#include "mp_tables.h"

//
// ACPI

// Root System Description Pointer (ACPI 1.0)
struct acpi_rsdp_t {
    char sig[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t rev;
    uint32_t rsdt_addr;
};

C_ASSERT(sizeof(acpi_rsdp_t) == 20);

// Root System Description Pointer (ACPI 2.0+)
struct acpi_rsdp2_t {
    acpi_rsdp_t rsdp1;

    uint32_t length;
    uint32_t xsdt_addr_lo;
    uint32_t xsdt_addr_hi;
    uint8_t checksum;
    uint8_t reserved[3];
};

C_ASSERT(sizeof(acpi_rsdp2_t) == 36);

// Root System Description Table
struct acpi_sdt_hdr_t {
    char sig[4];
    uint32_t len;
    uint8_t rev;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_rev;
    uint32_t creator_id;
    uint32_t creator_rev;
} __packed;

C_ASSERT(sizeof(acpi_sdt_hdr_t) == 36);

// Generic Address Structure
struct acpi_gas_t {
    uint8_t addr_space;
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t access_size;
    uint32_t addr_lo;
    uint32_t addr_hi;
};

C_ASSERT(sizeof(acpi_gas_t) == 12);

#define ACPI_GAS_ADDR_SYSMEM    0
#define ACPI_GAS_ADDR_SYSIO     1
#define ACPI_GAS_ADDR_PCICFG    2
#define ACPI_GAS_ADDR_EMBED     3
#define ACPI_GAS_ADDR_SMBUS     4
#define ACPI_GAS_ADDR_FIXED     0x7F

#define ACPI_GAS_ASZ_UNDEF  0
#define ACPI_GAS_ASZ_8      0
#define ACPI_GAS_ASZ_16     0
#define ACPI_GAS_ASZ_32     0
#define ACPI_GAS_ASZ_64     0

struct acpi_fadt_t {
    acpi_sdt_hdr_t hdr;
    uint32_t fw_ctl;
    uint32_t dsdt;

    // field used in ACPI 1.0; no longer in use, for compatibility only
    uint8_t  reserved;

    uint8_t  preferred_pm_profile;
    uint16_t sci_irq;
    uint32_t smi_cmd_port;
    uint8_t  acpi_enable;
    uint8_t  acpi_disable;
    uint8_t  s4_bios_req;
    uint8_t  pstate_ctl;
    uint32_t pm1a_event_block;
    uint32_t pm1b_event_block;
    uint32_t pm1a_ctl_block;
    uint32_t pm1b_ctl_block;
    uint32_t pm2_ctl_block;
    uint32_t pm_timer_block;
    uint32_t gpe0_block;
    uint32_t gpe1_block;
    uint8_t  pm1_event_len;
    uint8_t  pm1_ctl_len;
    uint8_t  pm2_ctl_len;
    uint8_t  pm_timer_len;
    uint8_t  gpe0_len;
    uint8_t  gpe1_len;
    uint8_t  gpe1_base;
    uint8_t  cstate_ctl;
    uint16_t worst_c2_lat;
    uint16_t worst_c3_lat;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t  duty_ofs;
    uint8_t  duty_width;
    uint8_t  day_alarm;
    uint8_t  month_alarm;
    uint8_t  century;

    // reserved in ACPI 1.0; used since ACPI 2.0+
    uint16_t boot_arch_flags;

    uint8_t  reserved2;

    // ACPI_FADT_FFF_*
    uint32_t flags;

    // 12 byte structure; see below for details
    acpi_gas_t reset_reg;

    uint8_t  reset_value;
    uint8_t  reserved3[3];

    // 64bit pointers - Available on ACPI 2.0+
    uint64_t x_fw_ctl;
    uint64_t x_dsdt;

    acpi_gas_t x_pm1a_event_block;
    acpi_gas_t x_pm1b_event_block;
    acpi_gas_t x_pm1a_control_block;
    acpi_gas_t x_pm1b_control_block;
    acpi_gas_t x_pm2Control_block;
    acpi_gas_t x_pm_timer_block;
    acpi_gas_t x_gpe0_block;
    acpi_gas_t x_gpe1_block;
} __packed;

//
// FADT Fixed Feature Flags

#define ACPI_FADT_FFF_WBINVD_BIT                0
#define ACPI_FADT_FFF_WBINVD_FLUSH_BIT          1
#define ACPI_FADT_FFF_PROC_C1_BIT               2
#define ACPI_FADT_FFF_P_LVL2_MP_BIT             3
#define ACPI_FADT_FFF_PWR_BUTTON_BIT            4
#define ACPI_FADT_FFF_SLP_BUTTON_BIT            5
#define ACPI_FADT_FFF_FIX_RTC_BIT               6
#define ACPI_FADT_FFF_RTC_S4_BIT                7
#define ACPI_FADT_FFF_TMR_VAL_EXT_BIT           8
#define ACPI_FADT_FFF_DCK_CAP_BIT               9
#define ACPI_FADT_FFF_RESET_REG_SUP_BIT         10
#define ACPI_FADT_FFF_SEALED_CASE_BIT           11
#define ACPI_FADT_FFF_HEADLESS_BIT              12
#define ACPI_FADT_FFF_CPU_SW_SLP_BIT            13
#define ACPI_FADT_FFF_PCI_EXP_WAK_BIT           14
#define ACPI_FADT_FFF_PLAT_CLOCK_BIT            15
#define ACPI_FADT_FFF_S4_RTC_STS_BIT            16
#define ACPI_FADT_FFF_REMOTE_ON_CAP_BIT         17
#define ACPI_FADT_FFF_FORCE_CLUSTER_BIT         18
#define ACPI_FADT_FFF_FORCE_PHYS_DEST_BIT       19
#define ACPI_FADT_FFF_HW_REDUCED_ACPI_BIT       20
#define ACPI_FADT_FFF_LOCAL_POWER_S0_CAP_BIT    21

#define ACPI_FADT_FFF_WBINVD \
    (1U<<ACPI_FADT_FFF_WBINVD_BIT)
#define ACPI_FADT_FFF_WBINVD_FLUSH \
    (1U<<ACPI_FADT_FFF_WBINVD_FLUSH_BIT)
#define ACPI_FADT_FFF_PROC_C1 \
    (1U<<ACPI_FADT_FFF_PROC_C1_BIT)
#define ACPI_FADT_FFF_P_LVL2_MP \
    (1U<<ACPI_FADT_FFF_P_LVL2_MP_BIT)
#define ACPI_FADT_FFF_PWR_BUTTON \
    (1U<<ACPI_FADT_FFF_PWR_BUTTON_BIT)
#define ACPI_FADT_FFF_SLP_BUTTON \
    (1U<<ACPI_FADT_FFF_SLP_BUTTON_BIT)
#define ACPI_FADT_FFF_FIX_RTC \
    (1U<<ACPI_FADT_FFF_FIX_RTC_BIT)
#define ACPI_FADT_FFF_RTC_S4 \
    (1U<<ACPI_FADT_FFF_RTC_S4_BIT)
#define ACPI_FADT_FFF_TMR_VAL_EXT \
    (1U<<ACPI_FADT_FFF_TMR_VAL_EXT_BIT)
#define ACPI_FADT_FFF_DCK_CAP \
    (1U<<ACPI_FADT_FFF_DCK_CAP_BIT)
#define ACPI_FADT_FFF_RESET_REG_SUP \
    (1U<<ACPI_FADT_FFF_RESET_REG_SUP_BIT)
#define ACPI_FADT_FFF_SEALED_CASE \
    (1U<<ACPI_FADT_FFF_SEALED_CASE_BIT)
#define ACPI_FADT_FFF_HEADLESS \
    (1U<<ACPI_FADT_FFF_HEADLESS_BIT)
#define ACPI_FADT_FFF_CPU_SW_SLP \
    (1U<<ACPI_FADT_FFF_CPU_SW_SLP_BIT)
#define ACPI_FADT_FFF_PCI_EXP_WAK \
    (1U<<ACPI_FADT_FFF_PCI_EXP_WAK_BIT)
#define ACPI_FADT_FFF_PLAT_CLOCK \
    (1U<<ACPI_FADT_FFF_PLAT_CLOCK_BIT)
#define ACPI_FADT_FFF_S4_RTC_STS \
    (1U<<ACPI_FADT_FFF_S4_RTC_STS_BIT)
#define ACPI_FADT_FFF_REMOTE_ON_CAP \
    (1U<<ACPI_FADT_FFF_REMOTE_ON_CAP_BIT)
#define ACPI_FADT_FFF_FORCE_CLUSTER \
    (1U<<ACPI_FADT_FFF_FORCE_CLUSTER_BIT)
#define ACPI_FADT_FFF_FORCE_PHYS_DEST \
    (1U<<ACPI_FADT_FFF_FORCE_PHYS_DEST_BIT)
#define ACPI_FADT_FFF_HW_REDUCED_ACPI \
    (1U<<ACPI_FADT_FFF_HW_REDUCED_ACPI_BIT)
#define ACPI_FADT_FFF_LOCAL_POWER_S0_CAP \
    (1U<<ACPI_FADT_FFF_LOCAL_POWER_S0_CAP_BIT)


struct acpi_ssdt_t {
    // sig == ?
    acpi_sdt_hdr_t hdr;
} __packed;

struct acpi_madt_rec_hdr_t {
    uint8_t entry_type;
    uint8_t record_len;
};

#define ACPI_MADT_REC_TYPE_LAPIC    0
#define ACPI_MADT_REC_TYPE_IOAPIC   1
#define ACPI_MADT_REC_TYPE_IRQ      2

struct acpi_madt_lapic_t {
    acpi_madt_rec_hdr_t hdr;
    uint8_t cpu_id;
    uint8_t apic_id;
    uint32_t flags;
} __packed;

struct acpi_madt_ioapic_t {
    acpi_madt_rec_hdr_t hdr;
    uint8_t apic_id;
    uint8_t reserved;
    uint32_t addr;
    uint32_t irq_base;
} __packed;

struct acpi_madt_irqsrc_t {
    acpi_madt_rec_hdr_t hdr;
    uint8_t bus;
    uint8_t irq_src;
    uint32_t gsi;
    uint16_t flags;
} __packed;

//
// The IRQ routing flags are identical to MPS flags

#define ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_BIT \
    MP_INTR_FLAGS_POLARITY_BIT
#define ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_BITS \
    MP_INTR_FLAGS_POLARITY_BITS
#define ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_BIT \
    MP_INTR_FLAGS_TRIGGER_BIT
#define ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_BITS \
    MP_INTR_FLAGS_TRIGGER_BITS

#define ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_MASK \
    MP_INTR_FLAGS_TRIGGER_MASK

#define ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_MASK \
    MP_INTR_FLAGS_POLARITY_MASK

#define ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER \
    MP_INTR_FLAGS_TRIGGER

#define ACPI_MADT_ENT_IRQ_FLAGS_POLARITY \
    MP_INTR_FLAGS_POLARITY

#define ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_DEFAULT \
    MP_INTR_FLAGS_POLARITY_DEFAULT
#define ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_ACTIVEHI \
    MP_INTR_FLAGS_POLARITY_ACTIVEHI
#define ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_ACTIVELO \
    MP_INTR_FLAGS_POLARITY_ACTIVELO

#define ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_DEFAULT \
    MP_INTR_FLAGS_TRIGGER_DEFAULT
#define ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_EDGE \
    MP_INTR_FLAGS_TRIGGER_EDGE
#define ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_LEVEL \
    MP_INTR_FLAGS_TRIGGER_LEVEL

#define ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_n(n) \
    MP_INTR_FLAGS_POLARITY_n(n)
#define ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_n(n) \
    MP_INTR_FLAGS_TRIGGER_n(n)

union acpi_madt_ent_t {
    acpi_madt_rec_hdr_t hdr;
    acpi_madt_lapic_t lapic;
    acpi_madt_ioapic_t ioapic;
    acpi_madt_irqsrc_t irq_src;
} __packed;

struct acpi_madt_t {
    // sig == "APIC"
    acpi_sdt_hdr_t hdr;

    uint32_t lapic_address;

    // 1 = Dual 8259 PICs installed
    uint32_t flags;
} __packed;

#define ACPI_MADT_FLAGS_HAVE_PIC_BIT    0
#define ACPI_MADT_FLAGS_HAVE_PIC        (1<<ACPI_MADT_FLAGS_HAVE_PIC_BIT)

//
// HPET ACPI info

struct acpi_hpet_t {
    acpi_sdt_hdr_t hdr;

    uint32_t blk_id;
    acpi_gas_t addr;
    uint8_t number;
    uint16_t min_tick_count;
    uint8_t page_prot;
};

#define ACPI_HPET_BLKID_PCI_VEN_BIT     16
#define ACPI_HPET_BLKID_LEGACY_CAP_BIT  15
#define ACPI_HPET_BLKID_COUNTER_SZ_BIT  13
#define ACPI_HPET_BLKID_NUM_CMP_BIT     8
#define ACPI_HPET_BLKID_REV_ID_BIT      0

#define ACPI_HPET_BLKID_PCI_VEN_BITS    16
#define ACPI_HPET_BLKID_NUM_CMP_BITS    8
#define ACPI_HPET_BLKID_REV_ID_BITS     8

#define ACPI_HPET_BLKID_PCI_VEN_MASK \
    ((1<<ACPI_HPET_BLKID_PCI_VEN_BITS)-1)
#define ACPI_HPET_BLKID_NUM_CMP_MASK \
    ((1<<ACPI_HPET_BLKID_NUM_CMP_BITS)-1)
#define ACPI_HPET_BLKID_REV_ID_MASK \
    ((1<<ACPI_HPET_BLKID_REV_ID_BITS)-1)

// LegacyReplacement IRQ Routing Capable
#define ACPI_HPET_BLKID_LEGACY_CAP \
    (1<<ACPI_HPET_BLKID_LEGACY_CAP_BIT)

// 64-bit counters
#define ACPI_HPET_BLKID_COUNTER_SZ \
    (1<<ACPI_HPET_BLKID_COUNTER_SZ_BIT)

// PCI Vendor ID
#define ACPI_HPET_BLKID_PCI_VEN \
    (ACPI_HPET_BLKID_PCI_VEN_MASK<<ACPI_HPET_BLKID_PCI_VEN_BIT)

// Number of comparators in 1st block
#define ACPI_HPET_BLKID_NUM_CMP \
    (ACPI_HPET_BLKID_NUM_CMP_MASK<<ACPI_HPET_BLKID_NUM_CMP_BIT)

// Hardware revision ID
#define ACPI_HPET_BLKID_REV_ID \
    (ACPI_HPET_BLKID_REV_ID_BITS<<ACPI_HPET_BLKID_REV_ID_BIT)
