#include "apic.h"
#include "types.h"
#include "gdt.h"
#include "bios_data.h"
#include "control_regs.h"
#include "interrupts.h"
#include "irq.h"
#include "idt.h"
#include "thread_impl.h"
#include "mm.h"
#include "cpuid.h"
#include "string.h"
#include "atomic.h"
#include "printk.h"
#include "likely.h"
#include "time.h"
#include "cpuid.h"
#include "spinlock.h"
#include "assert.h"
#include "bitsearch.h"
#include "callout.h"
#include "device/pci.h"
#include "ioport.h"
#include "mm.h"
#include "vector.h"
#include "bootinfo.h"
#include "acpi_tables.h"
#include "apic_local.h"
#include "apic_io.h"

#define ENABLE_ACPI 1
#define ENABLE_MPS  0

#define DEBUG_ACPI  1
#if DEBUG_ACPI
#define ACPI_TRACE(...) printk("acpi: " __VA_ARGS__)
#else
#define ACPI_TRACE(...) ((void)0)
#endif

#define ACPI_ERROR(...) printk("mp: error: " __VA_ARGS__)

#define DEBUG_MPS  1
#if DEBUG_MPS
#define MPS_TRACE(...) printk("mp: " __VA_ARGS__)
#else
#define MPS_TRACE(...) ((void)0)
#endif

#define MPS_ERROR(...) printk("mp: " __VA_ARGS__)

#define DEBUG_IOAPIC  1
#if DEBUG_IOAPIC
#define IOAPIC_TRACE(...) printk("ioapic: " __VA_ARGS__)
#else
#define IOAPIC_TRACE(...) ((void)0)
#endif

#define DEBUG_SMP  1
#if DEBUG_SMP
#define SMP_TRACE(...) printk("smp: " __VA_ARGS__)
#else
#define SMP_TRACE(...) ((void)0)
#endif

#define SMP_ERROR(...) printk("smp error: " __VA_ARGS__)

struct smp_data_t {
    vector<uint8_t> pci_bus_ids;
    uint16_t mp_isa_bus_id;
    vector<mp_bus_irq_mapping_t> bus_irq_list;
    uint8_t bus_irq_to_mapping[64];
    uint64_t apic_timer_freq;
    uint64_t rdtsc_mhz;
    uint64_t clk_to_ns_numer;
    uint64_t clk_to_ns_denom;
    unsigned ioapic_count;
    spinlock_t ioapic_msi_alloc_lock;
    uint8_t ioapic_msi_next_irq = INTR_APIC_IRQ_BASE;
    static uint64_t ioapic_msi_alloc_map[4];

    uint8_t ioapic_next_irq_base = 16;

    // First vector after last IOAPIC
    uint8_t ioapic_msi_base_intr;
    uint8_t ioapic_msi_base_irq;

    uint8_t isa_irq_lookup[16];

    uint32_t apic_base;

    vector<uint8_t> apic_id_list;

    uint8_t topo_thread_bits;
    uint8_t topo_thread_count;
    uint8_t topo_core_bits;
    uint8_t topo_core_count;

    uint8_t topo_cpu_count;

    vector<acpi_gas_t> acpi_hpet_list;
    int acpi_madt_flags;

    acpi_fadt_t acpi_fadt;

    uint64_t acpi_rsdt_addr;
    uint64_t acpi_rsdt_len;
    uint8_t acpi_sdt_bits;
};

smp_data_t smp;

static char *mp_tables;

// INT 0-31, 32-48, 128, and 255 initially "allocated"
uint64_t smp_data_t::ioapic_msi_alloc_map[] = {
    0x0000FFFFFFFFFFFFL,
    0x0000000000000000L,
    0x0000000000000001L,
    0x8000000000000000L
};

// The ACPI PM timer runs at 3.579545MHz
#define ACPI_PM_TIMER_HZ    3579545

int acpi_have8259pic(void)
{
    return !smp.acpi_rsdt_addr ||
            !!(smp.acpi_madt_flags & ACPI_MADT_FLAGS_HAVE_PIC);
}

static __always_inline uint8_t checksum_bytes(char const *bytes, size_t len)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < len; ++i)
        sum += (uint8_t)bytes[i];
    return sum;
}

static void acpi_process_fadt(acpi_fadt_t *fadt_hdr)
{
    smp.acpi_fadt = *fadt_hdr;
}

static void acpi_process_madt(acpi_madt_t *madt_hdr)
{
    acpi_madt_ent_t *ent = (acpi_madt_ent_t*)(madt_hdr + 1);
    acpi_madt_ent_t *end = (acpi_madt_ent_t*)
            ((char*)madt_hdr + madt_hdr->hdr.len);

    smp.apic_base = madt_hdr->lapic_address;
    smp.acpi_madt_flags = madt_hdr->flags & 1;

    vector<acpi_madt_ent_t*> ioapics;

    for ( ; ent < end;
          ent = (acpi_madt_ent_t*)((char*)ent + ent->ioapic.hdr.record_len)) {
        switch (ent->hdr.entry_type) {
        case ACPI_MADT_REC_TYPE_LAPIC:
            // If processor is enabled
            if (ent->lapic.flags == 1)
                smp.apic_id_list.emplace_back(ent->lapic.apic_id);
            else
                ACPI_TRACE("Disabled processor detected\n");
            break;

        case ACPI_MADT_REC_TYPE_IOAPIC:
            ioapics.emplace_back(ent);
            break;

        case ACPI_MADT_REC_TYPE_IRQ:
            if (smp.bus_irq_list.empty()) {
                // Populate bus_irq_list with 16 legacy ISA IRQs
                for (int i = 0; i < 16; ++i) {
                    mp_bus_irq_mapping_t mapping{};
                    mapping.bus = smp.mp_isa_bus_id;
                    mapping.intin = i;
                    mapping.irq = i;
                    mapping.flags = ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_ACTIVEHI |
                            ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_EDGE;
                    smp.isa_irq_lookup[i] = i;
                    smp.bus_irq_list.push_back(mapping);
                }
            }

            mp_bus_irq_mapping_t &mapping = smp.bus_irq_list[ent->irq_src.gsi];
            mapping.bus = ent->irq_src.bus;
            mapping.irq = ent->irq_src.irq_src;
            mapping.flags = ent->irq_src.flags;
            smp.isa_irq_lookup[ent->irq_src.irq_src] = ent->irq_src.gsi;
            break;
        }
    }

    ioapic_t *ioapic_array = ioapic_t::alloc(ioapics.size());
    for (size_t i = 0; i < ioapics.size(); ++i) {
        ioapic_array[i].init(ioapics[i]->ioapic.apic_id,
                             ioapics[i]->ioapic.addr,
                             ioapics[i]->ioapic.irq_base);
    }
}

static void acpi_process_hpet(acpi_hpet_t *acpi_hdr)
{
    smp.acpi_hpet_list.push_back(acpi_hdr->addr);
}

static __always_inline uint8_t acpi_chk_hdr(acpi_sdt_hdr_t *hdr)
{
    return checksum_bytes((char const *)hdr, hdr->len);
}

static bool acpi_find_rsdp(void *start, size_t len)
{
    for (size_t offset = 0; offset < len; offset += 16) {
        acpi_rsdp2_t *rsdp2 = (acpi_rsdp2_t*)
                ((char*)start + offset);

        // Check for ACPI 2.0+ RSDP
        if (!memcmp(rsdp2->rsdp1.sig, "RSD PTR ", 8)) {
            // Check checksum
            if (rsdp2->rsdp1.rev != 0 &&
                    checksum_bytes((char*)rsdp2,
                               sizeof(*rsdp2)) == 0 &&
                    checksum_bytes((char*)&rsdp2->rsdp1,
                                   sizeof(rsdp2->rsdp1)) == 0) {
                if (rsdp2->xsdt_addr_lo | rsdp2->xsdt_addr_hi) {
                    smp.acpi_rsdt_addr = rsdp2->xsdt_addr_lo |
                            ((uint64_t)rsdp2->xsdt_addr_hi << 32);
                    smp.acpi_sdt_bits = 64;
                } else {
                    smp.acpi_rsdt_addr = rsdp2->rsdp1.rsdt_addr;
                    smp.acpi_sdt_bits = 32;
                }

                smp.acpi_rsdt_len = rsdp2->length;

                printk("Found %u-bit ACPI 2.0+ RSDP at 0x%zx\n",
                       smp.acpi_sdt_bits, smp.acpi_rsdt_addr);

                return true;
            }
        }

        // Check for ACPI 1.0 RSDP
        acpi_rsdp_t *rsdp = (acpi_rsdp_t*)rsdp2;
        if (rsdp->rev == 0 &&
                !memcmp(rsdp->sig, "RSD PTR ", 8)) {
            // Check checksum
            if (checksum_bytes((char*)rsdp, sizeof(*rsdp)) == 0) {
                smp.acpi_rsdt_addr = rsdp->rsdt_addr;
                smp.acpi_sdt_bits = 32;

                // Leave acpi_rsdt_len 0 in this case, it is
                // handled later

                printk("Found ACPI 1.0 RSDP at 0x%zx\n", smp.acpi_rsdt_addr);

                return true;
            }
        }
    }

    return false;
}

static bool mp_find_fps(void *start, size_t len)
{
    for (size_t offset = 0; offset < len; offset += 16) {
        mp_table_hdr_t *sig_srch = (mp_table_hdr_t*)
                ((char*)start + offset);

        // Check for MP tables signature
        if (!memcmp(sig_srch->sig, "_MP_", 4)) {
            // Check checksum
            if (checksum_bytes((char*)sig_srch,
                               sizeof(*sig_srch)) == 0) {
                mp_tables = (char*)(uintptr_t)sig_srch->phys_addr;

                printk("Found MPS tables at %zx\n", size_t(mp_tables));

                return true;
            }
        }
    }

    return false;
}

// Sometimes we have to guess the size
// then read the header to get the actual size.
// This handles that.
template<typename T>
static T *acpi_remap_len(T *ptr, uintptr_t physaddr,
                         size_t guess, size_t actual_len)
{
    if (actual_len > guess) {
        munmap(ptr, guess);
        ptr = (T*)mmap((void*)physaddr, actual_len,
                       PROT_READ, MAP_PHYSICAL, -1, 0);
    }

    return ptr;
}

static void acpi_parse_rsdt()
{
    acpi_sdt_hdr_t *rsdt_hdr = (acpi_sdt_hdr_t *)mmap(
                (void*)smp.acpi_rsdt_addr,
                smp.acpi_rsdt_len ? smp.acpi_rsdt_len : sizeof(*rsdt_hdr),
                PROT_READ, MAP_PHYSICAL, -1, 0);

    // For ACPI 1.0, get length from header and remap
    if (!smp.acpi_rsdt_len) {
        smp.acpi_rsdt_len = rsdt_hdr->len;
        rsdt_hdr = acpi_remap_len(rsdt_hdr, smp.acpi_rsdt_addr,
                                      sizeof(*rsdt_hdr), smp.acpi_rsdt_len);
    }

    printk("RSDT version %x\n", rsdt_hdr->rev);

    if (acpi_chk_hdr(rsdt_hdr) != 0) {
        ACPI_ERROR("ACPI RSDT checksum mismatch!\n");
        return;
    }

    void *rsdp_ptrs = (rsdt_hdr + 1);
    void *rsdp_end = ((char*)rsdt_hdr + rsdt_hdr->len);

    printk("Processing RSDP pointers from %p to %p\n",
           (void*)rsdp_ptrs, (void*)rsdp_end);

    for (void *rsdp_ptr = rsdp_ptrs;
         rsdp_ptr < rsdp_end;
         rsdp_ptr = (char*)rsdp_ptr + (smp.acpi_sdt_bits >> 3)) {
        uint64_t hdr_addr;
        if (smp.acpi_sdt_bits == 64)
            hdr_addr = *(uint64_t*)rsdp_ptr;
        else
            hdr_addr = *(uint32_t*)rsdp_ptr;

        acpi_sdt_hdr_t *hdr = (acpi_sdt_hdr_t *)
                mmap((void*)(uintptr_t)hdr_addr,
                      64 << 10, PROT_READ, MAP_PHYSICAL, -1, 0);

        hdr = acpi_remap_len(hdr, hdr_addr, 64 << 10, hdr->len);

        if (!memcmp(hdr->sig, "FACP", 4)) {
            acpi_fadt_t *fadt_hdr = (acpi_fadt_t *)hdr;

            if (acpi_chk_hdr(&fadt_hdr->hdr) == 0) {
                ACPI_TRACE("ACPI FADT found\n");
                acpi_process_fadt(fadt_hdr);
            } else {
                ACPI_ERROR("ACPI FADT checksum mismatch!\n");
            }
        } else if (!memcmp(hdr->sig, "APIC", 4)) {
            acpi_madt_t *madt_hdr = (acpi_madt_t *)hdr;

            if (acpi_chk_hdr(&madt_hdr->hdr) == 0) {
                ACPI_TRACE("ACPI MADT found\n");
                acpi_process_madt(madt_hdr);
            } else {
                ACPI_ERROR("ACPI MADT checksum mismatch!\n");
            }
        } else if (!memcmp(hdr->sig, "HPET", 4)) {
            acpi_hpet_t *hpet_hdr = (acpi_hpet_t *)hdr;

            if (acpi_chk_hdr(&hpet_hdr->hdr) == 0) {
                ACPI_TRACE("ACPI HPET found\n");
                acpi_process_hpet(hpet_hdr);
            } else {
                ACPI_ERROR("ACPI MADT checksum mismatch!\n");
            }
        } else {
            if (acpi_chk_hdr(hdr) == 0) {
                ACPI_TRACE("ACPI %4.4s ignored\n", hdr->sig);
            } else {
                ACPI_ERROR("ACPI %4.4s checksum mismatch!"
                       " (ignored anyway)\n", hdr->sig);
            }
        }

        munmap(hdr, max(size_t(64 << 10), size_t(hdr->len)));
    }
}

#if ENABLE_MPS
static void mp_parse_fps()
{
    mp_cfg_tbl_hdr_t *cth = (mp_cfg_tbl_hdr_t *)
            mmap(mp_tables, 0x10000,
                 PROT_READ, MAP_PHYSICAL, -1, 0);

    cth = acpi_remap_len(cth, uintptr_t(mp_tables),
                         0x10000, cth->base_tbl_len + cth->ext_tbl_len);

    uint8_t *entry = (uint8_t*)(cth + 1);

    // Reset to impossible values
    smp.mp_isa_bus_id = -1;

    // First slot reserved for BSP
    smp.apic_id_list.emplace_back();

    vector<mp_cfg_ioapic_t*> ioapics;

    for (uint16_t i = 0; i < cth->entry_count; ++i) {
        mp_cfg_cpu_t *entry_cpu;
        mp_cfg_bus_t *entry_bus;
        mp_cfg_ioapic_t *entry_ioapic;
        mp_cfg_iointr_t *entry_iointr;
        mp_cfg_lintr_t *entry_lintr;
        mp_cfg_addrmap_t *entry_addrmap;
        mp_cfg_bushier_t *entry_busheir;
        mp_cfg_buscompat_t *entry_buscompat;
        switch (*entry) {
        case MP_TABLE_TYPE_CPU:
        {
            entry_cpu = (mp_cfg_cpu_t *)entry;

            MPS_TRACE("CPU package found,"
                      " base apic id=%u ver=0x%x\n",
                      entry_cpu->apic_id, entry_cpu->apic_ver);

            if ((entry_cpu->flags & MP_CPU_FLAGS_ENABLED)) {
                if (entry_cpu->flags & MP_CPU_FLAGS_BSP)
                    smp.apic_id_list[0] = entry_cpu->apic_id;
                else
                    smp.apic_id_list.emplace_back(entry_cpu->apic_id);
            } else {
                ACPI_ERROR("CPU not enabled\n");
            }

            entry = (uint8_t*)(entry_cpu + 1);
            break;
        }

        case MP_TABLE_TYPE_BUS:
        {
            entry_bus = (mp_cfg_bus_t *)entry;

            MPS_TRACE("%.*s bus found, id=%u\n",
                     (int)sizeof(entry_bus->type),
                     entry_bus->type, entry_bus->bus_id);

            if (!memcmp(entry_bus->type, "PCI   ", 6)) {
                smp.pci_bus_ids.push_back(entry_bus->bus_id);
            } else if (!memcmp(entry_bus->type, "ISA   ", 6)) {
                if (smp.mp_isa_bus_id == 0xFFFF)
                    smp.mp_isa_bus_id = entry_bus->bus_id;
                else
                    MPS_ERROR("Too many ISA busses,"
                              " only one supported\n");
            } else {
                MPS_ERROR("Dropped! Unrecognized bus named \"%.*s\"\n",
                          (int)sizeof(entry_bus->type), entry_bus->type);
            }
            entry = (uint8_t*)(entry_bus + 1);
            break;
        }

        case MP_TABLE_TYPE_IOAPIC:
        {
            entry_ioapic = (mp_cfg_ioapic_t *)entry;

            if (entry_ioapic->flags & MP_IOAPIC_FLAGS_ENABLED)
                ioapics.emplace_back(entry_ioapic);

            entry = (uint8_t*)(entry_ioapic + 1);
            break;
        }

        case MP_TABLE_TYPE_IOINTR:
        {
            entry_iointr = (mp_cfg_iointr_t *)entry;

            if (memchr(smp.pci_bus_ids.data(), entry_iointr->source_bus,
                        smp.pci_bus_ids.size())) {
                // PCI IRQ
                uint8_t bus = entry_iointr->source_bus;
                uint8_t intr_type = entry_iointr->type;
                uint8_t intr_flags = entry_iointr->flags;
                uint8_t device = entry_iointr->source_bus_irq >> 2;
                uint8_t pci_irq = entry_iointr->source_bus_irq & 3;
                uint8_t ioapic_id = entry_iointr->dest_ioapic_id;
                uint8_t intin = entry_iointr->dest_ioapic_intin;

                MPS_TRACE("PCI device %u INT_%c# ->"
                          " IOAPIC ID 0x%02x INTIN %d %s\n",
                          device, (int)(pci_irq) + 'A',
                          ioapic_id, intin,
                          intr_type < intr_type_text_count ?
                              intr_type_text[intr_type] :
                              "(invalid type!)");


                mp_bus_irq_mapping_t mapping{};
                mapping.bus = bus;
                mapping.intr_type = intr_type;
                mapping.flags = intr_flags;
                mapping.device = device;
                mapping.irq = pci_irq & 3;
                mapping.ioapic_id = ioapic_id;
                mapping.intin = intin;
                smp.bus_irq_list.push_back(mapping);
            } else if (entry_iointr->source_bus == smp.mp_isa_bus_id) {
                // ISA IRQ

                uint8_t bus =  entry_iointr->source_bus;
                uint8_t intr_type = entry_iointr->type;
                uint8_t intr_flags = entry_iointr->flags;
                uint8_t isa_irq = entry_iointr->source_bus_irq;
                uint8_t ioapic_id = entry_iointr->dest_ioapic_id;
                uint8_t intin = entry_iointr->dest_ioapic_intin;

                MPS_TRACE("ISA IRQ %d -> IOAPIC ID 0x%02x INTIN %u %s\n",
                          isa_irq, ioapic_id, intin,
                          intr_type < intr_type_text_count ?
                              intr_type_text[intr_type] :
                              "(invalid type!)");

                smp.isa_irq_lookup[isa_irq] = smp.bus_irq_list.size();
                mp_bus_irq_mapping_t mapping{};
                mapping.bus = bus;
                mapping.intr_type = intr_type;
                mapping.flags = intr_flags;
                mapping.device = 0;
                mapping.irq = isa_irq;
                mapping.ioapic_id = ioapic_id;
                mapping.intin = intin;
                smp.bus_irq_list.push_back(mapping);
            } else {
                // Unknown bus!
                MPS_ERROR("IRQ %d on unknown bus ->"
                          " IOAPIC ID 0x%02x INTIN %u type=%d\n",
                          entry_iointr->source_bus_irq,
                          entry_iointr->dest_ioapic_id,
                          entry_iointr->dest_ioapic_intin,
                          entry_iointr->type);
            }

            entry = (uint8_t*)(entry_iointr + 1);
            break;
        }

        case MP_TABLE_TYPE_LINTR:
        {
            entry_lintr = (mp_cfg_lintr_t*)entry;
            if (memchr(smp.pci_bus_ids.data(),
                       entry_lintr->source_bus, smp.pci_bus_ids.size())) {
                uint8_t device = entry_lintr->source_bus_irq >> 2;
                uint8_t pci_irq = entry_lintr->source_bus_irq;
                uint8_t lapic_id = entry_lintr->dest_lapic_id;
                uint8_t intin = entry_lintr->dest_lapic_lintin;
                MPS_TRACE("PCI device %u INT_%c# ->"
                          " LAPIC ID 0x%02x INTIN %d\n",
                          device, (int)(pci_irq & 3) + 'A',
                          lapic_id, intin);
            } else if (entry_lintr->source_bus == smp.mp_isa_bus_id) {
                uint8_t isa_irq = entry_lintr->source_bus_irq;
                uint8_t lapic_id = entry_lintr->dest_lapic_id;
                uint8_t intin = entry_lintr->dest_lapic_lintin;

                MPS_TRACE("ISA IRQ %d -> LAPIC ID 0x%02x INTIN %u\n",
                          isa_irq, lapic_id, intin);
            } else {
                // Unknown bus!
                MPS_ERROR("IRQ %d on unknown bus ->"
                          " IOAPIC ID 0x%02x INTIN %u\n",
                          entry_lintr->source_bus_irq,
                          entry_lintr->dest_lapic_id,
                          entry_lintr->dest_lapic_lintin);
            }
            entry = (uint8_t*)(entry_lintr + 1);
            break;
        }

        case MP_TABLE_TYPE_ADDRMAP:
        {
            entry_addrmap = (mp_cfg_addrmap_t*)entry;
            uint8_t bus = entry_addrmap->bus_id;
            uint64_t addr =  entry_addrmap->addr_lo |
                    ((uint64_t)entry_addrmap->addr_hi << 32);
            uint64_t len =  entry_addrmap->addr_lo |
                    ((uint64_t)entry_addrmap->addr_hi << 32);

            MPS_TRACE("Address map, bus=%d, addr=%lx, len=%lx\n",
                      bus, addr, len);

            entry += entry_addrmap->len;
            break;
        }

        case MP_TABLE_TYPE_BUSHIER:
        {
            entry_busheir = (mp_cfg_bushier_t*)entry;
            uint8_t bus = entry_busheir->bus_id;
            uint8_t parent_bus = entry_busheir->parent_bus;
            uint8_t info = entry_busheir->info;

            MPS_TRACE("Bus hierarchy, bus=%d, parent=%d, info=%x\n",
                      bus, parent_bus, info);

            entry += entry_busheir->len;
            break;
        }

        case MP_TABLE_TYPE_BUSCOMPAT:
        {
            entry_buscompat = (mp_cfg_buscompat_t*)entry;
            uint8_t bus = entry_buscompat->bus_id;
            uint8_t bus_mod = entry_buscompat->bus_mod;
            uint32_t bus_predef = entry_buscompat->predef_range_list;

            MPS_TRACE("Bus compat, bus=%d, mod=%d,"
                      " predefined_range_list=%x\n",
                      bus, bus_mod, bus_predef);

            entry += entry_buscompat->len;
            break;
        }

        default:
            MPS_ERROR("Unknown MP table entry_type!"
                      " Guessing size is 8\n");
            // Hope for the best here
            entry += 8;
            break;
        }
    }

    ioapic_t *ioapic_list = ioapic_t::alloc(ioapics.size());

    uint8_t ioapic_next_irq = 48;

    for (size_t i = 0; i < ioapics.size(); ++i)
    {
        mp_cfg_ioapic_t *entry_ioapic = ioapics[i];
        ioapic_t *ioapic = ioapic_list + i;

        MPS_TRACE("IOAPIC id=%d, addr=0x%x,"
                  " flags=0x%x, ver=0x%x\n",
                  entry_ioapic->id,
                  entry_ioapic->addr,
                  entry_ioapic->flags,
                  entry_ioapic->ver);

        ioapic->init(entry_ioapic->id,
                     entry_ioapic->addr,
                     ioapic_next_irq);

        ioapic_next_irq += ioapic->irq_count();
    }

    munmap(cth, max(0x10000, cth->base_tbl_len + cth->ext_tbl_len));
}
#endif

int smp_parse_tables(void)
{
    uint16_t ebda_seg = *BIOS_DATA_AREA(uint16_t, 0x40E);
    uintptr_t ebda = uintptr_t(ebda_seg << 4);

    // ACPI RSDP can be found:
    //  - in the first 1KB of the EBDA
    //  - in the 128KB range starting at 0xE0000

    // MP table floating pointer structure can be found:
    //  - in the first 1KB of the EBDA
    //  - in the 1KB range starting at 0x9FC00
    //  - in the 64KB range starting at 0xF0000

    void *p_9fc00 = mmap((void*)0x9FC00, 0x400, PROT_READ,
                         MAP_PHYSICAL, -1, 0);
    void *p_e0000 = mmap((void*)0xE0000, 0x20000, PROT_READ,
                       MAP_PHYSICAL, -1, 0);
    void *p_f0000 = (char*)p_e0000 + 0x10000;

    void *p_ebda;
    if (ebda == 0x9FC00) {
        p_ebda = p_9fc00;
    } else {
        p_ebda = mmap((void*)ebda, 0x400, PROT_READ,
                      MAP_PHYSICAL, -1, 0);
    }

    struct range {
        void *start;
        size_t len;
        bool (*search_fn)(void *start, size_t len);
    } const search_data[] = {
#if ENABLE_ACPI
        range{ p_ebda, 0x400, acpi_find_rsdp },
        range{ p_e0000, 0x20000, acpi_find_rsdp },
#endif
#if ENABLE_MPS
        range{ p_ebda, 0x400, mp_find_fps },
        range{ p_9fc00 != p_ebda ? p_9fc00 : nullptr, 0x400, mp_find_fps },
        range{ p_f0000, 0x10000, mp_find_fps }
#endif
    };

    for (size_t i = 0; i < countof(search_data); ++i) {
        if (unlikely(!search_data[i].start))
            continue;
        if (search_data[i].search_fn(
                    search_data[i].start, search_data[i].len))
            break;
    }

#if ENABLE_ACPI
    if (smp.acpi_rsdt_addr)
        acpi_parse_rsdt();
    else
#endif
#if ENABLE_MPS
    if (mp_tables)
        mp_parse_fps();
    else
#endif
        SMP_ERROR("Can't find ACPI tables or MP tables! Can't run SMP\n");

    munmap(p_9fc00, 0x400);
    munmap(p_e0000, 0x20000);
    if (p_ebda != p_9fc00)
        munmap(p_ebda, 0x400);

    return !!mp_tables;
}

static void apic_calibrate();

static void apic_detect_topology_amd(void)
{

}

static void apic_detect_topology_intel(void)
{
    cpuid_t info;

    if (!cpuid(&info, CPUID_TOPOLOGY1, 0)) {
        // Enable full CPUID
        uint64_t misc_enables = msr_get(MSR_MISC_ENABLES);
        if (misc_enables & (1L<<22)) {
            // Enable more CPUID support and retry
            misc_enables &= ~(1L<<22);
            msr_set(MSR_MISC_ENABLES, misc_enables);
        }
    }

    smp.topo_thread_bits = 0;
    smp.topo_core_bits = 0;
    smp.topo_thread_count = 1;
    smp.topo_core_count = 1;

    if (cpuid(&info, CPUID_INFO_FEATURES, 0)) {
        if ((info.edx >> 28) & 1) {
            // CPU supports hyperthreading

            // Thread count
            smp.topo_thread_count = (info.ebx >> 16) & 0xFF;
            while ((1U << smp.topo_thread_bits) < smp.topo_thread_count)
                 ++smp.topo_thread_bits;
        }

        if (cpuid(&info, CPUID_TOPOLOGY1, 0)) {
            smp.topo_core_count = ((info.eax >> 26) & 0x3F) + 1;
            while ((1U << smp.topo_core_bits) < smp.topo_core_count)
                 ++smp.topo_core_bits;
        }
    }

    if (smp.topo_thread_bits >= smp.topo_core_bits)
        smp.topo_thread_bits -= smp.topo_core_bits;
    else
        smp.topo_thread_bits = 0;

    smp.topo_thread_count /= smp.topo_core_count;

    // Workaround strange occurrence of it calculating 0 threads
    if (smp.topo_thread_count <= 0)
        smp.topo_thread_count = 1;

    smp.topo_cpu_count = smp.apic_id_list.size() *
            smp.topo_core_count * smp.topo_thread_count;
}

static void apic_detect_topology(void)
{
    apic_detect_topology_intel();
}

void apic_start_smp(void)
{
    // Start the timer here because interrupts are enable by now
    apic->configure_timer(smp.apic_timer_freq / 60,
                          lapic_t::lvt_tr_mode_t::PERIODIC,
                         INTR_APIC_TIMER, 0);

    SMP_TRACE("%zd CPU packages\n", smp.apic_id_list.size());

    if (!smp.acpi_rsdt_addr) {
        apic_detect_topology();
    } else {
        // Treat as N cpu packages
        smp.topo_cpu_count = smp.apic_id_list.size();
        smp.topo_core_bits = 0;
        smp.topo_thread_bits = 0;
        smp.topo_core_count = 1;
        smp.topo_thread_count = 1;
    }

    gdt_init_tss(smp.topo_cpu_count);
    gdt_load_tr(0);

    // See if there are any other CPUs to start
    if (smp.topo_thread_count * smp.topo_core_count == 1 &&
            smp.apic_id_list.size() == 1)
        return;

    // Read address of MP entry trampoline from boot sector
    //uint32_t *mp_trampoline_ptr = (uint32_t*)0x7c40;
    //		bootinfo_parameter(bootparam_t::ap_entry_point);
    uint32_t mp_trampoline_addr = //*mp_trampoline_ptr;
            (uint32_t)
            bootinfo_parameter(bootparam_t::ap_entry_point);
    uint32_t mp_trampoline_page = mp_trampoline_addr >> 12;

    // Send INIT to all other CPUs
    apic->broadcast_init();

    sleep(10);

    SMP_TRACE("%d hyperthread bits\n", smp.topo_thread_bits);
    SMP_TRACE("%d core bits\n", smp.topo_core_bits);

    SMP_TRACE("%d hyperthread count\n", smp.topo_thread_count);
    SMP_TRACE("%d core count\n", smp.topo_core_count);

    uint32_t smp_expect = 0;
    for (unsigned pkg = 0; pkg < smp.apic_id_list.size(); ++pkg) {
        SMP_TRACE("Package base APIC ID = %u\n", smp.apic_id_list[pkg]);

        uint8_t total_cpus = smp.topo_core_count *
                smp.topo_thread_count *
                smp.apic_id_list.size();
        uint32_t stagger = 16666666 / total_cpus;

        for (unsigned thread = 0;
             thread < smp.topo_thread_count; ++thread) {
            for (unsigned core = 0; core < smp.topo_core_count; ++core) {
                uint8_t target = smp.apic_id_list[pkg] +
                        (thread | (core << smp.topo_thread_bits));

                // Don't try to start BSP
                if (target == smp.apic_id_list[0])
                    continue;

                SMP_TRACE("Sending IPI to APIC ID %u\n", target);

                // Send SIPI to CPU
                apic->send_sipi(target, mp_trampoline_page);

                nsleep(stagger);

                ++smp_expect;
                while (thread_smp_running != smp_expect)
                    pause();
            }
        }
    }

    // SMP online
    callout_call(callout_type_t::smp_online);

    ioapic_irq_cpu(0, 1);
}

//
// ACPI timer

class acpi_gas_accessor_t {
public:
    static acpi_gas_accessor_t *from_gas(acpi_gas_t const& gas);
    static acpi_gas_accessor_t *from_ioport(uint16_t ioport, int size);

    virtual ~acpi_gas_accessor_t() {}
    virtual size_t get_size() const = 0;
    virtual int64_t read() const = 0;
    virtual void write(int64_t value) const = 0;
};

template<int size>
class acpi_gas_accessor_sysmem_t : public acpi_gas_accessor_t {
public:
    typedef typename type_from_size<size, true>::type value_type;

    acpi_gas_accessor_sysmem_t(uint64_t mem_addr)
    {
        mem = (value_type*)mmap((void*)mem_addr, size,
                                PROT_READ | PROT_WRITE,
                                MAP_PHYSICAL, -1, 0);
    }

    size_t get_size() const final { return size; }

    int64_t read() const final { return *mem; }

    void write(int64_t value) const final
    {
        *mem = value_type(value);
    }

private:
    value_type *mem;
};

template<int size>
class acpi_gas_accessor_sysio_t : public acpi_gas_accessor_t {
public:
    typedef typename type_from_size<size, true>::type value_type;

    acpi_gas_accessor_sysio_t(uint64_t io_port)
        : port(ioport_t(io_port)) {}

    size_t get_size() const final { return size; }

    int64_t read() const final { return inp<size>(port); }

    void write(int64_t value) const final
    {
        outp<size>(port, value_type(value));
    }

private:
    ioport_t port;
};

template<int size>
class acpi_gas_accessor_pcicfg_t : public acpi_gas_accessor_t {
public:
    typedef typename type_from_size<size, true>::type value_type;

    acpi_gas_accessor_pcicfg_t(uint64_t pci_dfo)
        : dfo(pci_dfo) {}

    size_t get_size() const final { return size; }

    int64_t read() const final
    {
        value_type result;
        pci_config_copy(0, (dfo >> 32) & 0xFF,
                        (dfo >> 16) & 0xFF, &result,
                        dfo & 0xFF, size);
        return result;
    }

    void write(int64_t value) const final
    {
        pci_config_write(0, (dfo >> 32) & 0xFF,
                         (dfo >> 16) & 0xFF, dfo & 0xFF, &value, size);
    }

private:
    uint64_t dfo;
};

//template<int size>
//class acpi_gas_accessor_embed_t : public acpi_gas_accessor_t {
//public:
//    typedef typename type_from_size<size, true>::type value_type;
//
//    acpi_gas_accessor_embed_t(uint64_t addr, uint8_t size)
//        : acpi_gas_accessor_t(size) {}
//};
//
//template<int size>
//class acpi_gas_accessor_smbus_t : public acpi_gas_accessor_t {
//public:
//    typedef type_from_size<size, true>::type value_type;
//
//    acpi_gas_accessor_smbus_t(uint64_t addr, uint8_t size)
//        : acpi_gas_accessor_t(size) {}
//};
//
//template<int size>
//class acpi_gas_accessor_fixed_t : public acpi_gas_accessor_t {
//public:
//    typedef type_from_size<size, true>::type value_type;
//
//    acpi_gas_accessor_fixed_t(uint64_t addr, uint8_t size)
//        : acpi_gas_accessor_t(size) {}
//};

static uint64_t acpi_pm_timer_nsleep_handler(uint64_t nanosec);

// Returns -1 if PM timer is not available, otherwise a 24 or 32 bit raw value
static int64_t acpi_pm_timer_raw()
{
    static acpi_gas_accessor_t *accessor;

    if (unlikely(!accessor &&
                 (smp.acpi_fadt.pm_timer_block ||
                  smp.acpi_fadt.x_pm_timer_block.access_size))) {
        if (likely(smp.acpi_fadt.pm_timer_block)) {
            ACPI_TRACE("PM Timer at I/O port 0x%x\n",
                       smp.acpi_fadt.pm_timer_block);
            accessor = new acpi_gas_accessor_sysio_t<4>(
                        smp.acpi_fadt.pm_timer_block);
        } else if (smp.acpi_fadt.x_pm_timer_block.access_size) {
            accessor = acpi_gas_accessor_t::from_gas(
                        smp.acpi_fadt.x_pm_timer_block);
        }

        if (likely(accessor))
            nsleep_set_handler(acpi_pm_timer_nsleep_handler, nullptr, true);
    }

    if (likely(accessor))
        return accessor->read();

    return -1;
}

static uint32_t acpi_pm_timer_diff(uint32_t before, uint32_t after)
{
    // If counter is 32 bits
    if (likely(smp.acpi_fadt.flags & ACPI_FADT_FFF_TMR_VAL_EXT))
        return after - before;

    // Counter is 24 bits
    return ((after << 8) - (before << 8)) >> 8;
}

// Timer precision is approximately 279ns
static uint64_t acpi_pm_timer_ns(uint32_t diff)
{
    return (uint64_t(diff) * 1000000000) / ACPI_PM_TIMER_HZ;
}

__used
static uint64_t acpi_pm_timer_nsleep_handler(uint64_t ns)
{
    uint32_t st = acpi_pm_timer_raw();
    uint32_t en;
    uint32_t elap;
    uint32_t elap_ns;
    do {
        en = acpi_pm_timer_raw();
        elap = acpi_pm_timer_diff(st, en);
        elap_ns = acpi_pm_timer_ns(elap);
    } while (elap_ns < ns);

    return elap_ns;
}

template<typename T>
constexpr T gcd(T a, T b)
{
    if (b)
        return gcd(b, a % b);
    return a;
}

static uint64_t apic_rdtsc_time_ns_handler()
{
    uint64_t now = cpu_rdtsc();
    return now * smp.clk_to_ns_numer/ smp.clk_to_ns_denom;
}

//
// MSI IRQ

// Returns the starting IRQ number of allocated range
// Returns 0 for failure
int apic_msi_irq_alloc(msi_irq_mem_t *results, int count,
                       int cpu, bool distribute,
                       intr_handler_t handler)
{
    // Don't try to use MSI if there are no IOAPIC devices
    if (!ioapic_t::count())
        return 0;

    // If out of range starting CPU number, force to zero
    if (cpu < 0 || (unsigned)cpu >= smp.apic_id_list.size())
        cpu = 0;

    uint8_t vector_base = ioapic_t::aligned_vectors(bit_log2(count));

    // See if we ran out of vectors
    if (vector_base == 0)
        return 0;

    for (int i = 0; i < count; ++i) {
        results[i].addr = (0xFEEU << 20) |
                (smp.apic_id_list[cpu] << 12);
        results[i].data = (vector_base + i);

        irq_hook(vector_base + i - smp.ioapic_msi_base_intr +
                 smp.ioapic_msi_base_irq,
                 handler);

        // It will always be edge triggered
        // (!!activehi << 14) |
        // (!!level << 15);
    }

    return vector_base - INTR_APIC_IRQ_BASE;
}

acpi_gas_accessor_t *acpi_gas_accessor_t::from_gas(acpi_gas_t const& gas)
{
    uint64_t addr = gas.addr_lo | (uint64_t(gas.addr_hi) << 32);

    ACPI_TRACE("Using extended PM Timer Generic Address Structure: "
               " addr_space: 0x%x, addr=0x%lx, size=0x%x,"
               " width=0x%x, bit=0x%x\n",
               gas.addr_space, addr, gas.access_size,
               gas.bit_width, gas.bit_offset);

    switch (gas.addr_space) {
    case ACPI_GAS_ADDR_SYSMEM:
        ACPI_TRACE("ACPI PM Timer using MMIO address space: 0x%lx\n", addr);
        switch (gas.access_size) {
        case 1: return new acpi_gas_accessor_sysmem_t<1>(addr);
        case 2: return new acpi_gas_accessor_sysmem_t<2>(addr);
        case 4: return new acpi_gas_accessor_sysmem_t<4>(addr);
        case 8: return new acpi_gas_accessor_sysmem_t<8>(addr);
        default: return nullptr;
        }
    case ACPI_GAS_ADDR_SYSIO:
        ACPI_TRACE("ACPI PM Timer using I/O address space: 0x%lx\n", addr);
        switch (gas.access_size) {
        case 1: return new acpi_gas_accessor_sysio_t<1>(addr);
        case 2: return new acpi_gas_accessor_sysio_t<2>(addr);
        case 4: return new acpi_gas_accessor_sysio_t<4>(addr);
        case 8: return nullptr;
        default: return nullptr;
        }
    case ACPI_GAS_ADDR_PCICFG:
        ACPI_TRACE("ACPI PM Timer using PCI config address space: 0x%lx\n",
                   addr);
        switch (gas.access_size) {
        case 1: return new acpi_gas_accessor_pcicfg_t<1>(addr);
        case 2: return new acpi_gas_accessor_pcicfg_t<2>(addr);
        case 4: return new acpi_gas_accessor_pcicfg_t<4>(addr);
        case 8: return new acpi_gas_accessor_pcicfg_t<8>(addr);
        default: return nullptr;
        }
    case ACPI_GAS_ADDR_EMBED: // fall thru
    case ACPI_GAS_ADDR_SMBUS: // fall thru
    case ACPI_GAS_ADDR_FIXED: // fall thru
    default:
        ACPI_TRACE("Unhandled ACPI PM Timer address space: 0x%x\n",
                   gas.addr_space);
        return nullptr;
    }
}
