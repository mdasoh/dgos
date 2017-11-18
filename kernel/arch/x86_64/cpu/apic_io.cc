#include "apic_io.h"
#include "mm.h"
#include "printk.h"
#include "mutex.h"

//
// IOAPIC registers

#define IOAPIC_IOREGSEL         0
#define IOAPIC_IOREGWIN         4

#define IOAPIC_REG_ID           0
#define IOAPIC_REG_VER          1
#define IOAPIC_REG_ARB          2

#define IOAPIC_VER_VERSION_BIT  0
#define IOAPIC_VER_VERSION_BITS 8
#define IOAPIC_VER_VERSION_MASK ((1<<IOAPIC_VER_VERSION_BITS)-1)
#define IOAPIC_VER_VERSION      \
    (IOAPIC_VER_VERSION_MASK<<IOAPIC_VER_VERSION_BIT)
#define IOAPIC_VER_VERSION_n(n) ((n)<<IOAPIC_VER_VERSION_BIT)

#define IOAPIC_VER_ENTRIES_BIT  16
#define IOAPIC_VER_ENTRIES_BITS 8
#define IOAPIC_VER_ENTRIES_MASK ((1<<IOAPIC_VER_ENTRIES_BITS)-1)
#define IOAPIC_VER_ENTRIES      \
    (IOAPIC_VER_ENTRIES_MASK<<IOAPIC_VER_ENTRIES_BIT)
#define IOAPIC_VER_ENTRIES_n(n) ((n)<<IOAPIC_VER_ENTRIES_BIT)

#define IOAPIC_RED_LO_n(n)      (0x10 + (n) * 2)
#define IOAPIC_RED_HI_n(n)      (0x10 + (n) * 2 + 1)

#define IOAPIC_REDLO_VECTOR_BIT     0
#define IOAPIC_REDLO_DELIVERY_BIT   8
#define IOAPIC_REDLO_DESTMODE_BIT   11
#define IOAPIC_REDLO_STATUS_BIT     12
#define IOAPIC_REDLO_POLARITY_BIT   13
#define IOAPIC_REDLO_REMOTEIRR_BIT  14
#define IOAPIC_REDLO_TRIGGER_BIT    15
#define IOAPIC_REDLO_MASKIRQ_BIT    16
#define IOAPIC_REDHI_DEST_BIT       (56-32)

#define IOAPIC_REDLO_DESTMODE       (1<<IOAPIC_REDLO_DESTMODE_BIT)
#define IOAPIC_REDLO_STATUS         (1<<IOAPIC_REDLO_STATUS_BIT)
#define IOAPIC_REDLO_POLARITY       (1<<IOAPIC_REDLO_POLARITY_BIT)
#define IOAPIC_REDLO_REMOTEIRR      (1<<IOAPIC_REDLO_REMOTEIRR_BIT)
#define IOAPIC_REDLO_TRIGGER        (1<<IOAPIC_REDLO_TRIGGER_BIT)
#define IOAPIC_REDLO_MASKIRQ        (1<<IOAPIC_REDLO_MASKIRQ_BIT)

#define IOAPIC_REDLO_VECTOR_BITS    8
#define IOAPIC_REDLO_DELVERY_BITS   3
#define IOAPIC_REDHI_DEST_BITS      8

#define IOAPIC_REDLO_VECTOR_MASK    ((1<<IOAPIC_REDLO_VECTOR_BITS)-1)
#define IOAPIC_REDLO_DELVERY_MASK   ((1<<IOAPIC_REDLO_DELVERY_BITS)-1)
#define IOAPIC_REDHI_DEST_MASK      ((1<<IOAPIC_REDHI_DEST_BITS)-1)

#define IOAPIC_REDLO_VECTOR \
    (IOAPIC_REDLO_VECTOR_MASK<<IOAPIC_REDLO_VECTOR_BITS)
#define IOAPIC_REDLO_DELVERY \
    (IOAPIC_REDLO_DELVERY_MASK<<IOAPIC_REDLO_DELVERY_BITS)
#define IOAPIC_REDHI_DEST \
    (IOAPIC_REDHI_DEST_MASK<<IOAPIC_REDHI_DEST_BITS)

#define IOAPIC_REDLO_VECTOR_n(n)    ((n)<<IOAPIC_REDLO_VECTOR_BIT)
#define IOAPIC_REDLO_DELIVERY_n(n)  ((n)<<IOAPIC_REDLO_DELIVERY_BIT)
#define IOAPIC_REDLO_TRIGGER_n(n)   ((n)<<IOAPIC_REDLO_TRIGGER_BIT)
#define IOAPIC_REDHI_DEST_n(n)      ((n)<<IOAPIC_REDHI_DEST_BIT)
#define IOAPIC_REDLO_POLARITY_n(n)  ((n)<<IOAPIC_REDLO_POLARITY_BIT)

#define IOAPIC_REDLO_DELIVERY_APIC      0
#define IOAPIC_REDLO_DELIVERY_LOWPRI    1
#define IOAPIC_REDLO_DELIVERY_SMI       2
#define IOAPIC_REDLO_DELIVERY_NMI       4
#define IOAPIC_REDLO_DELIVERY_INIT      5
#define IOAPIC_REDLO_DELIVERY_EXTINT    7

#define IOAPIC_REDLO_TRIGGER_EDGE   0
#define IOAPIC_REDLO_TRIGGER_LEVEL  1

#define IOAPIC_REDLO_POLARITY_ACTIVELO  1
#define IOAPIC_REDLO_POLARITY_ACTIVEHI  0


static uint8_t ioapic_alloc_vectors(uint8_t count)
{
    spinlock_lock_noirq(&ioapic_msi_alloc_lock);

    uint8_t base = ioapic_msi_next_irq;

    for (size_t intr = base - INTR_APIC_IRQ_BASE, end = intr + count;
         intr < end; ++intr) {
        ioapic_msi_alloc_map[intr >> 6] |= (1UL << (intr & 0x3F));
    }

    spinlock_unlock_noirq(&ioapic_msi_alloc_lock);

    return base;
}

//
// IOAPIC

static ioapic_t *ioapic_by_id(uint8_t id)
{
    for (unsigned i = 0; i < ioapic_count; ++i) {
        if (ioapic_list[i].id == id)
            return ioapic_list + i;
    }

    printk("Failed to find ioapic by id=%d, idlist=", id);
    for (unsigned i = 0; i < ioapic_count; ++i) {
        printk("%u%s", ioapic_list[i].id,
               (i+1) != ioapic_count ? "," : "");
    }
    panic("IOAPIC not found");
    return nullptr;
}

// Returns 1 on success
// device should be 0 for ISA IRQs
static void ioapic_map(mp_ioapic_t *ioapic,
                       mp_bus_irq_mapping_t *mapping)
{
    uint8_t delivery;

    printk("Mapping IOAPIC physaddr=%x ptr=%p",
           ioapic->addr, (void*)ioapic->ptr);

    switch (mapping->intr_type) {
    case MP_INTR_TYPE_APIC:
        delivery = IOAPIC_REDLO_DELIVERY_APIC;
        break;

    case MP_INTR_TYPE_NMI:
        delivery = IOAPIC_REDLO_DELIVERY_NMI;
        break;

    case MP_INTR_TYPE_SMI:
        delivery = IOAPIC_REDLO_DELIVERY_SMI;
        break;

    case MP_INTR_TYPE_EXTINT:
        delivery = IOAPIC_REDLO_DELIVERY_EXTINT;
        break;

    default:
        APIC_ERROR("MP: Unrecognized interrupt delivery type!"
                   " Guessing APIC\n");
        delivery = IOAPIC_REDLO_DELIVERY_APIC;
        break;
    }

    uint8_t polarity;

    switch (mapping->flags & ACPI_MADT_ENT_IRQ_FLAGS_POLARITY) {
    default:
    case ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_n(
            ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_ACTIVEHI):
        polarity = IOAPIC_REDLO_POLARITY_ACTIVEHI;
        break;
    case ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_n(
            ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_ACTIVELO):
        polarity = IOAPIC_REDLO_POLARITY_ACTIVELO;
        break;
    }

    uint8_t trigger;

    switch (mapping->flags & ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER) {
    default:
        APIC_TRACE("MP: Unrecognized IRQ trigger type!"
                   " Guessing edge\n");
        // fall through...
    case ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_n(
            ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_DEFAULT):
    case ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_n(
            ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_EDGE):
        trigger = IOAPIC_REDLO_TRIGGER_EDGE;
        break;

    case ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_n(
            ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_LEVEL):
        trigger = IOAPIC_REDLO_TRIGGER_LEVEL;
        break;

    }

    uint8_t intr = ioapic->base_intr + mapping->intin;

    uint32_t iored_lo =
            IOAPIC_REDLO_VECTOR_n(intr) |
            IOAPIC_REDLO_DELIVERY_n(delivery) |
            IOAPIC_REDLO_POLARITY_n(polarity) |
            IOAPIC_REDLO_TRIGGER_n(trigger);

    IOAPIC_TRACE("Mapped IOAPIC irq %d (intin=%d) to interrupt 0x%x\n",
                 mapping->irq, mapping->intin, intr);

    uint32_t iored_hi = IOAPIC_REDHI_DEST_n(0);

    ioapic_lock_noirq(ioapic);

    // Write low part with mask set
    ioapic_write(ioapic, IOAPIC_RED_LO_n(mapping->intin),
                 iored_lo | IOAPIC_REDLO_MASKIRQ);

    atomic_barrier();

    // Write high part
    ioapic_write(ioapic, IOAPIC_RED_HI_n(mapping->intin), iored_hi);

    atomic_barrier();

    ioapic_unlock_noirq(ioapic);
}

//
//

static mp_ioapic_t *ioapic_from_intr(int intr)
{
    for (unsigned i = 0; i < ioapic_count; ++i) {
        mp_ioapic_t *ioapic = ioapic_list + i;
        if (intr >= ioapic->base_intr &&
                intr < ioapic->base_intr + ioapic->vector_count) {
            return ioapic;
        }
    }
    return nullptr;
}

static mp_bus_irq_mapping_t *ioapic_mapping_from_irq(int irq)
{
    return &bus_irq_list[bus_irq_to_mapping[irq]];
}

static isr_context_t *ioapic_dispatcher(int intr, isr_context_t *ctx)
{
    isr_context_t *orig_ctx = ctx;

    uint8_t irq;
    if (intr >= ioapic_msi_base_intr &&
            intr < INTR_APIC_SPURIOUS) {
        // MSI IRQ
        irq = intr - ioapic_msi_base_intr + ioapic_msi_base_irq;
    } else {
        mp_ioapic_t *ioapic = ioapic_from_intr(intr);
        assert(ioapic);
        if (!ioapic)
            return ctx;

        // IOAPIC IRQ

        mp_bus_irq_mapping_t *mapping = bus_irq_list.data();
        uint8_t intin = intr - ioapic->base_intr;
        for (irq = 0; irq < bus_irq_list.size(); ++irq, ++mapping) {
            if (mapping->ioapic_id == ioapic->id &&
                    mapping->intin == intin)
                break;
        }

        // Reverse map ISA IRQ
        uint8_t *isa_match = (uint8_t *)memchr(isa_irq_lookup, irq,
                                               sizeof(isa_irq_lookup));

        if (isa_match)
            irq = isa_match - isa_irq_lookup;
        else
            irq = ioapic->irq_base + intin;
    }

    apic_eoi(intr);
    ctx = (isr_context_t*)irq_invoke(intr, irq, ctx);

    if (ctx == orig_ctx)
        return thread_schedule_if_idle(ctx);

    return ctx;
}

static void ioapic_setmask(int irq, bool unmask)
{
    IOAPIC_TRACE("Setting IRQ mask for %d to %s\n",
                 irq, unmask ? "unmasked" : "masked");

    mp_bus_irq_mapping_t *mapping = ioapic_mapping_from_irq(irq);
    mp_ioapic_t *ioapic = ioapic_by_id(mapping->ioapic_id);

    ioapic_lock_noirq(ioapic);

    uint32_t ent = ioapic_read(ioapic, IOAPIC_RED_LO_n(mapping->intin));

    if (unmask)
        ent &= ~IOAPIC_REDLO_MASKIRQ;
    else
        ent |= IOAPIC_REDLO_MASKIRQ;

    ioapic_write(ioapic, IOAPIC_RED_LO_n(mapping->intin), ent);

    ioapic_unlock_noirq(ioapic);
}

static void ioapic_hook(int irq, intr_handler_t handler)
{
    IOAPIC_TRACE("Hooking IRQ %d, handler=%p\n", irq, (void*)handler);

    uint8_t intr;
    if (irq >= ioapic_msi_base_irq) {
        // MSI IRQ
        intr = irq - ioapic_msi_base_irq + ioapic_msi_base_intr;
    } else {
        // IOAPIC IRQ
        mp_bus_irq_mapping_t *mapping = ioapic_mapping_from_irq(irq);
        mp_ioapic_t *ioapic = ioapic_by_id(mapping->ioapic_id);
        intr = ioapic->base_intr + mapping->intin;
    }

    intr_hook(intr, handler);
}

static void ioapic_unhook(int irq, intr_handler_t handler)
{
    IOAPIC_TRACE("Unhooking IRQ %d, handler=%p\n", irq, (void*)handler);

    mp_bus_irq_mapping_t *mapping = ioapic_mapping_from_irq(irq);
    mp_ioapic_t *ioapic = ioapic_by_id(mapping->ioapic_id);
    uint8_t intr = ioapic->base_intr + mapping->intin;
    intr_unhook(intr, handler);
}

static void ioapic_map_all(void)
{
    IOAPIC_TRACE("Mapping interrupt inputs\n");

    mp_bus_irq_mapping_t *mapping;
    mp_ioapic_t *ioapic;

    for (unsigned i = 0; i < 16; ++i)
        bus_irq_to_mapping[i] = isa_irq_lookup[i];

    for (size_t i = 0; i < bus_irq_list.size(); ++i) {
        mapping = &bus_irq_list[i];
        ioapic = ioapic_by_id(mapping->ioapic_id);

        if (!ioapic) {
            IOAPIC_TRACE("Could not find IOAPIC for IRQ %u, skipping\n",
                         mapping->irq);
            continue;
        }

        if (mapping->bus != mp_isa_bus_id) {
            uint8_t irq = ioapic->irq_base + mapping->intin;
            bus_irq_to_mapping[irq] = i;
        }

        ioapic_map(ioapic, mapping);
    }

    // MSI IRQs start after last IOAPIC
    ioapic = &ioapic_list[ioapic_count-1];
    ioapic_msi_base_intr = ioapic->base_intr +
            ioapic->vector_count;
    ioapic_msi_base_irq = ioapic->irq_base +
            ioapic->vector_count;
}

// Pass negative cpu value to get highest CPU number
// Returns 0 on failure, 1 on success
bool ioapic_irq_cpu(int irq, int cpu)
{
    if (cpu < 0)
        return ioapic_count > 0;

    if (unsigned(cpu) >= apic_id_count)
        return false;

    IOAPIC_TRACE("Routing IRQ %d to CPU %d\n", irq, cpu);

    mp_bus_irq_mapping_t *mapping = ioapic_mapping_from_irq(irq);
    mp_ioapic_t *ioapic = ioapic_by_id(mapping->ioapic_id);
    ioapic_lock_noirq(ioapic);
    ioapic_write(ioapic, IOAPIC_RED_HI_n(mapping->intin),
                 IOAPIC_REDHI_DEST_n(smp.apic_id_list[cpu]));
    ioapic_unlock_noirq(ioapic);
    return true;
}

int apic_enable(void)
{
    if (ioapic_count == 0)
        return 0;

    ioapic_map_all();

    irq_dispatcher_set_handler(ioapic_dispatcher);
    irq_setmask_set_handler(ioapic_setmask);
    irq_hook_set_handler(ioapic_hook);
    irq_unhook_set_handler(ioapic_unhook);
    irq_setcpu_set_handler(ioapic_irq_cpu);

    return 1;
}

ioapic_t::ioapic_t()
    : lock(0)
{
}

ioapic_t::~ioapic_t()
{
    if (ptr)
        munmap(ptr, 12);
}

uint8_t ioapic_t::irq_count() const
{
    return entries;
}

size_t ioapic_t::count()
{
    return ioapic_count;
}

uint8_t ioapic_t::aligned_vectors(uint8_t log2n)
{
    int count = 1 << log2n;

    uint64_t mask = ~((uint64_t)-1 << count);
    uint64_t checked = mask;
    uint8_t result = 0;

    unique_lock<spinlock_t> hold(irq_allocator_lock);
    spinlock_lock_noirq(&irq_allocator_lock);

    for (size_t bit = 0; bit < 128; bit += count)
    {
        size_t i = bit >> 6;

        if (!(ioapic_msi_alloc_map[i] & checked)) {
            ioapic_msi_alloc_map[i] |= checked;
            result = bit + INTR_APIC_IRQ_BASE;
            break;
        }
        checked <<= count;
        if (!checked)
            checked = mask;
    }

    spinlock_unlock_noirq(&irq_allocator_lock);

    return result;
}

ioapic_t *ioapic_t::alloc(size_t count)
{
    ioapic_list = new ioapic_t[count];

}

bool ioapic_t::init(uint8_t id, uint32_t addr, uint8_t irq_base)
{
    this->addr = addr;
    this->id = id;
    this->irq_base = irq_base;

    ptr = (uint32_t *)mmap(
                (void*)(uintptr_t)addr,
                12, PROT_READ | PROT_WRITE,
                MAP_PHYSICAL |
                MAP_NOCACHE |
                MAP_WRITETHRU, -1, 0);

    entries = (read(IOAPIC_REG_VER) >> IOAPIC_VER_ENTRIES_BIT) &
            IOAPIC_VER_ENTRIES_MASK;

    intr_base = irq_allocator.alloc(entries);

    return ptr != MAP_FAILED;
}

uint32_t ioapic_t::read(uint32_t reg)
{
    ptr[IOAPIC_IOREGSEL] = reg;
    return ptr[IOAPIC_IOREGWIN];
}

void ioapic_t::write(uint32_t reg, uint32_t val)
{
    ptr[IOAPIC_IOREGSEL] = reg;
    ptr[IOAPIC_IOREGWIN] = val;
}

void ioapic_t::chgbits(uint32_t reg, uint32_t clr, uint32_t set)
{
    ptr[IOAPIC_IOREGSEL] = reg;
    uint32_t val = ptr[IOAPIC_IOREGWIN];
    val &= ~clr;
    val |= set;
    ptr[IOAPIC_IOREGWIN] = val;
}

void ioapic_t::set_unmask(uint8_t intin, bool unmask)
{
    chgbits(IOAPIC_RED_LO_n(intin),
           IOAPIC_REDLO_MASKIRQ, unmask * IOAPIC_REDLO_MASKIRQ);
}
