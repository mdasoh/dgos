#include "apic_local.h"
#include "interrupts.h"
#include "idt.h"
#include "assert.h"
#include "cpu/control_regs.h"
#include "printk.h"
#include "stdlib.h"
#include "mm.h"
#include "irq.h"

#define DEBUG_APIC  1
#if DEBUG_APIC
#define APIC_TRACE(...) printk("lapic: " __VA_ARGS__)
#else
#define APIC_TRACE(...) ((void)0)
#endif

#define APIC_ERROR(...) printk("lapic: error: " __VA_ARGS__)

//
// APIC

// APIC ID (read only)
#define APIC_REG_ID             0x02

// APIC version (read only)
#define APIC_REG_VER            0x03

// Task Priority Register
#define APIC_REG_TPR            0x08

// Arbitration Priority Register (not present in x2APIC mode)
#define APIC_REG_APR            0x09

// Processor Priority Register (read only)
#define APIC_REG_PPR            0x0A

// End Of Interrupt register (must write 0 in x2APIC mode)
#define APIC_REG_EOI            0x0B

// Logical Destination Register (not writeable in x2APIC mode)
#define APIC_REG_LDR            0x0D

// Destination Format Register (not present in x2APIC mode)
#define APIC_REG_DFR            0x0E

// Spurious Interrupt Register
#define APIC_REG_SIR            0x0F

// In Service Registers (bit n) (read only)
#define APIC_REG_ISR_n(n)       (0x10 + ((n) >> 5))

// Trigger Mode Registers (bit n) (read only)
#define APIC_REG_TMR_n(n)       (0x18 + ((n) >> 5))

// Interrupt Request Registers (bit n) (read only)
#define APIC_REG_IRR_n(n)       (0x20 + ((n) >> 5))

// Error Status Register (not present in x2APIC mode)
#define APIC_REG_ESR            0x28

// Local Vector Table Corrected Machine Check Interrupt
#define APIC_REG_LVT_CMCI       0x2F

// Local Vector Table Interrupt Command Register Lo (64-bit in x2APIC mode)
#define APIC_REG_ICR_LO         0x30

// Local Vector Table Interrupt Command Register Hi (not present in x2APIC mode)
#define APIC_REG_ICR_HI         0x31

// Local Vector Table Timer Register
#define APIC_REG_LVT_TR         0x32

// Local Vector Table Thermal Sensor Register
#define APIC_REG_LVT_TSR        0x33

// Local Vector Table Performance Monitoring Counter Register
#define APIC_REG_LVT_PMCR       0x34

// Local Vector Table Local Interrupt 0 Register
#define APIC_REG_LVT_LNT0       0x35

// Local Vector Table Local Interrupt 1 Register
#define APIC_REG_LVT_LNT1       0x36

// Local Vector Table Error Register
#define APIC_REG_LVT_ERR        0x37

// Local Vector Table Timer Initial Count Register
#define APIC_REG_LVT_ICR        0x38

// Local Vector Table Timer Current Count Register (read only)
#define APIC_REG_LVT_CCR        0x39

// Local Vector Table Timer Divide Configuration Register
#define APIC_REG_LVT_DCR        0x3E

// Self Interprocessor Interrupt (x2APIC only, write only)
#define APIC_REG_SELF_IPI       0x3F

// Only used in xAPIC mode
#define APIC_DEST_BIT               24
#define APIC_DEST_BITS              8
#define APIC_DEST_MASK              ((1U << APIC_DEST_BITS)-1U)
#define APIC_DEST_n(n)              \
    (((n) & APIC_DEST_MASK) << APIC_DEST_BIT)

#define APIC_CMD_SIPI_PAGE_BIT      0
#define APIC_CMD_VECTOR_BIT         0
#define APIC_CMD_DEST_MODE_BIT      8
#define APIC_CMD_DEST_LOGICAL_BIT   11
#define APIC_CMD_PENDING_BIT        12
#define APIC_CMD_ILD_CLR_BIT        14
#define APIC_CMD_ILD_SET_BIT        15
#define APIC_CMD_DEST_TYPE_BIT      18

#define APIC_CMD_VECTOR_BITS        8
#define APIC_CMD_SIPI_PAGE_BITS     8
#define APIC_CMD_DEST_MODE_BITS     3
#define APIC_CMD_DEST_TYPE_BITS     2

#define APIC_CMD_VECTOR_MASK        ((1 << APIC_CMD_VECTOR_BITS)-1)
#define APIC_CMD_SIPI_PAGE_MASK     ((1 << APIC_CMD_SIPI_PAGE_BITS)-1)
#define APIC_CMD_DEST_MODE_MASK     ((1 << APIC_CMD_DEST_MODE_BITS)-1)
#define APIC_CMD_DEST_TYPE_MASK     ((1 << APIC_CMD_DEST_TYPE_BITS)-1)

#define APIC_CMD_SIPI_PAGE      \
    (APIC_CMD_SIPI_PAGE_MASK << APIC_CMD_SIPI_PAGE_BIT)
#define APIC_CMD_DEST_MODE      \
    (APIC_CMD_DEST_MODE_MASK << APIC_CMD_DEST_MODE_BIT)
#define APIC_CMD_SIPI_PAGE      \
    (APIC_CMD_SIPI_PAGE_MASK << APIC_CMD_SIPI_PAGE_BIT)

#define APIC_CMD_SIPI_PAGE_n(n) ((n) << APIC_CMD_SIPI_PAGE_BIT)
#define APIC_CMD_DEST_MODE_n(n) ((n) << APIC_CMD_DEST_MODE_BIT)
#define APIC_CMD_DEST_TYPE_n(n) ((n) << APIC_CMD_DEST_TYPE_BIT)
#define APIC_CMD_SIPI_PAGE_n(n) ((n) << APIC_CMD_SIPI_PAGE_BIT)

#define APIC_CMD_VECTOR_n(n)    \
    (((n) & APIC_CMD_VECTOR_MASK) << APIC_CMD_VECTOR_BIT)

#define APIC_CMD_VECTOR         (1U << APIC_CMD_VECTOR_BIT)
#define APIC_CMD_DEST_LOGICAL   (1U << APIC_CMD_DEST_LOGICAL_BIT)
#define APIC_CMD_PENDING        (1U << APIC_CMD_PENDING_BIT)
#define APIC_CMD_ILD_CLR        (1U << APIC_CMD_ILD_CLR_BIT)
#define APIC_CMD_ILD_SET        (1U << APIC_CMD_ILD_SET_BIT)
#define APIC_CMD_DEST_TYPE      (1U << APIC_CMD_DEST_TYPE_BIT)

#define APIC_CMD_DEST_MODE_NORMAL   APIC_CMD_DEST_MODE_n(0)
#define APIC_CMD_DEST_MODE_LOWPRI   APIC_CMD_DEST_MODE_n(1)
#define APIC_CMD_DEST_MODE_SMI      APIC_CMD_DEST_MODE_n(2)
#define APIC_CMD_DEST_MODE_NMI      APIC_CMD_DEST_MODE_n(4)
#define APIC_CMD_DEST_MODE_INIT     APIC_CMD_DEST_MODE_n(5)
#define APIC_CMD_DEST_MODE_SIPI     APIC_CMD_DEST_MODE_n(6)

#define APIC_CMD_DEST_TYPE_BYID     APIC_CMD_DEST_TYPE_n(0)
#define APIC_CMD_DEST_TYPE_SELF     APIC_CMD_DEST_TYPE_n(1)
#define APIC_CMD_DEST_TYPE_ALL      APIC_CMD_DEST_TYPE_n(2)
#define APIC_CMD_DEST_TYPE_OTHER    APIC_CMD_DEST_TYPE_n(3)

// Divide configuration register
#define APIC_LVT_DCR_BY_2           0
#define APIC_LVT_DCR_BY_4           1
#define APIC_LVT_DCR_BY_8           2
#define APIC_LVT_DCR_BY_16          3
#define APIC_LVT_DCR_BY_32          (8+0)
#define APIC_LVT_DCR_BY_64          (8+1)
#define APIC_LVT_DCR_BY_128         (8+2)
#define APIC_LVT_DCR_BY_1           (8+3)

#define APIC_SIR_APIC_ENABLE_BIT    8
#define APIC_SIR_APIC_ENABLE        (1<<APIC_SIR_APIC_ENABLE_BIT)

#define APIC_LVT_TR_MODE_BIT        17
#define APIC_LVT_TR_MODE_BITS       2
#define APIC_LVT_TR_MODE_MASK       ((1U<<APIC_LVT_TR_MODE_BITS)-1U)
#define APIC_LVT_TR_MODE_n(n)       ((n)<<APIC_LVT_TR_MODE_BIT)

#define APIC_LVT_MASK_BIT       16
#define APIC_LVT_PENDING_BIT    12
#define APIC_LVT_LEVEL_BIT      15
#define APIC_LVT_REMOTEIRR_BIT  14
#define APIC_LVT_ACTIVELOW_BIT  13
#define APIC_LVT_DELIVERY_BIT   8
#define APIC_LVT_DELIVERY_BITS  3

#define APIC_LVT_MASK           (1U<<APIC_LVT_MASK_BIT)
#define APIC_LVT_PENDING        (1U<<APIC_LVT_PENDING_BIT)
#define APIC_LVT_LEVEL          (1U<<APIC_LVT_LEVEL_BIT)
#define APIC_LVT_REMOTEIRR      (1U<<APIC_LVT_REMOTEIRR_BIT)
#define APIC_LVT_ACTIVELOW      (1U<<APIC_LVT_ACTIVELOW_BIT)
#define APIC_LVT_DELIVERY_MASK  ((1U<<APIC_LVT_DELIVERY_BITS)-1)
#define APIC_LVT_DELIVERY_n(n)  ((n)<<APIC_LVT_DELIVERY_BIT)

#define APIC_LVT_DELIVERY_FIXED 0
#define APIC_LVT_DELIVERY_SMI   2
#define APIC_LVT_DELIVERY_NMI   4
#define APIC_LVT_DELIVERY_EXINT 7
#define APIC_LVT_DELIVERY_INIT  5

#define APIC_LVT_VECTOR_BIT     0
#define APIC_LVT_VECTOR_BITS    8
#define APIC_LVT_VECTOR_MASK    ((1U<<APIC_LVT_VECTOR_BITS)-1U)
#define APIC_LVT_VECTOR_n(n)    ((n)<<APIC_LVT_VECTOR_BIT)

#define APIC_BASE_MSR  0x1B

#define APIC_BASE_ADDR_BIT      12
#define APIC_BASE_ADDR_BITS     40
#define APIC_BASE_GENABLE_BIT   11
#define APIC_BASE_X2ENABLE_BIT  10
#define APIC_BASE_BSP_BIT       8

#define APIC_BASE_GENABLE       (1UL<<APIC_BASE_GENABLE_BIT)
#define APIC_BASE_X2ENABLE      (1UL<<APIC_BASE_X2ENABLE_BIT)
#define APIC_BASE_BSP           (1UL<<APIC_BASE_BSP_BIT)
#define APIC_BASE_ADDR_MASK     ((1UL<<APIC_BASE_ADDR_BITS)-1)
#define APIC_BASE_ADDR          (APIC_BASE_ADDR_MASK<<APIC_BASE_ADDR_BIT)

class lapic_x_t : public lapic_t {
    void command(uint32_t dest, uint32_t cmd) const final
    {
        cpu_scoped_irq_disable intr_enabled;
        write32(APIC_REG_ICR_HI, APIC_DEST_n(dest));
        write32(APIC_REG_ICR_LO, cmd);
        intr_enabled.restore();
        while (read32(APIC_REG_ICR_LO) & APIC_CMD_PENDING)
            pause();
    }

    uint32_t read32(uint32_t offset) const final
    {
        return apic_ptr[offset << (4 - 2)];
    }

    void write32(uint32_t offset, uint32_t val) const final
    {
        apic_ptr[offset << (4 - 2)] = val;
    }

    uint64_t read64(uint32_t offset) const final
    {
        return ((uint64_t*)apic_ptr)[offset << (4 - 3)];
    }

    void write64(uint32_t offset, uint64_t val) const final
    {
        ((uint64_t*)apic_ptr)[offset << (4 - 3)] = val;
    }

    bool reg_readable(uint32_t reg) const final
    {
        return reg_maybe_readable(reg);
    }

    bool reg_exists(uint32_t reg) const final
    {
        return reg_maybe_readable(reg);
    }
};

class lapic_x2_t : public lapic_t {
    void command(uint32_t dest, uint32_t cmd) const final
    {
        write64(APIC_REG_ICR_LO, (uint64_t(dest) << 32) | cmd);
    }

    uint32_t read32(uint32_t offset) const final
    {
        return msr_get_lo(0x800 + offset);
    }

    void write32(uint32_t offset, uint32_t val) const final
    {
        msr_set(0x800 + offset, val);
    }

    uint64_t read64(uint32_t offset) const final
    {
        return msr_get(0x800 + offset);
    }

    void write64(uint32_t offset, uint64_t val) const final
    {
        msr_set(0x800 + offset, val);
    }

    bool reg_readable(uint32_t reg) const override final
    {
        // APIC_REG_LVT_CMCI raises #GP if CMCI not enabled
        return reg != APIC_REG_LVT_CMCI &&
                reg != APIC_REG_ICR_HI &&
                reg_exists(reg) &&
                reg_maybe_readable(reg);
    }

    bool reg_exists(uint32_t reg) const override final
    {
        return reg != APIC_REG_DFR &&
                reg != APIC_REG_APR &&
                reg_maybe_exists(reg);
    }

    void write_ldr(uint32_t val) const override final
    {
        // Do nothing
        // LDR is read only in x2APIC mode
    }
};

static isr_context_t *apic_spurious_handler(int intr, isr_context_t *ctx)
{
    (void)intr;
    assert(intr == INTR_APIC_SPURIOUS);
    APIC_ERROR("Spurious APIC interrupt!\n");
    return ctx;
}

union lapics_t {
    lapic_x_t apic_x;
    lapic_x2_t apic_x2;

    lapics_t() {}
} apic_instance;

int lapic_t::apic_init(int ap)
{
    uint64_t apic_base_msr = msr_get(APIC_BASE_MSR);

    if (!apic_base)
        apic_base = apic_base_msr & APIC_BASE_ADDR;

    if (!(apic_base_msr & APIC_BASE_GENABLE)) {
        printk("APIC was disabled! Enabling\n");

        APIC_TRACE("APIC was globally disabled!"
                   " Enabling...\n");
    }

    if (cpuid_has_x2apic()) {
        printk("Using x2APIC\n");
        APIC_TRACE("Using x2APIC\n");
        if (!ap)
            apic = new ((void*)&apic_instance.apic_x2) lapic_x2_t();

        msr_set(APIC_BASE_MSR, apic_base_msr |
                APIC_BASE_GENABLE | APIC_BASE_X2ENABLE);
    } else {
        APIC_TRACE("Using xAPIC\n");

        if (!ap) {
            // Bootstrap CPU only
            assert(!apic_ptr);
            apic_ptr = (uint32_t *)mmap(
                        (void*)apic_base,
                        4096, PROT_READ | PROT_WRITE,
                        MAP_PHYSICAL | MAP_NOCACHE |
                        MAP_WRITETHRU, -1, 0);

            apic = new (&apic_instance.apic_x) lapic_x_t();
        }

        msr_set(APIC_BASE_MSR, apic_base_msr | APIC_BASE_GENABLE);
    }

    // Set global enable if it is clear
    if (!(apic_base_msr & APIC_BASE_GENABLE)) {
        APIC_TRACE("APIC was globally disabled!"
                   " Enabling...\n");
    }

    apic_base_msr &= APIC_BASE_ADDR;

    if (!ap) {
        intr_hook(INTR_APIC_TIMER, apic_timer_handler);
        intr_hook(INTR_APIC_SPURIOUS, apic_spurious_handler);

        printk("Parsing boot tables\n");

        smp_parse_tables();

        printk("Calibrating APIC timer\n");

        apic->calibrate();
    }

    printk("Enabling APIC\n");

    apic->online(1, INTR_APIC_SPURIOUS);

    apic->write32(APIC_REG_TPR, 0x0);

    assert(apic_base == (msr_get(APIC_BASE_MSR) & APIC_BASE_ADDR));

    if (ap) {
        printk("Configuring APIC timer\n");
        configure_timer(APIC_LVT_DCR_BY_1,
                             smp.apic_timer_freq / 60,
                             APIC_LVT_TR_MODE_PERIODIC,
                             INTR_APIC_TIMER);
    }

    apic_dump_regs(ap);

    return 1;
}

void lapic_t::write_ldr(uint32_t val)
{
    write32(APIC_REG_LDR, val);
}

void lapic_t::eoi(int intr)
{
    write32(APIC_REG_EOI, intr & 0);
}

void lapic_t::online(int enabled, int spurious_intr)
{
    uint32_t sir = read32(APIC_REG_SIR);

    if (enabled)
        sir |= APIC_SIR_APIC_ENABLE;
    else
        sir &= ~APIC_SIR_APIC_ENABLE;

    if (spurious_intr >= 32)
        sir = (sir & -256) | spurious_intr;

    // LDR is read only in x2APIC mode
    write_ldr(0xFFFFFFFF);

    write32(APIC_REG_SIR, sir);
}

void lapic_t::broadcast_init()
{
    command(0xFFFFFFFF,
                  APIC_CMD_DEST_MODE_INIT |
                  APIC_CMD_DEST_LOGICAL |
            APIC_CMD_DEST_TYPE_OTHER);
}

void lapic_t::send_sipi(uint32_t target, uint32_t trampoline_page)
{
    command(target,
            APIC_CMD_SIPI_PAGE_n(trampoline_page) |
            APIC_CMD_DEST_MODE_SIPI | APIC_CMD_DEST_TYPE_BYID);
}

void lapic_t::send_ipi(int target_apic_id, uint8_t intr)
{
    uint32_t dest_type = (target_apic_id < -1)
            ? APIC_CMD_DEST_TYPE_ALL
            : (target_apic_id < 0)
              ? APIC_CMD_DEST_TYPE_OTHER
              : APIC_CMD_DEST_TYPE_BYID;

    uint32_t dest_mode = (intr != INTR_EX_NMI)
            ? APIC_CMD_DEST_MODE_NORMAL
            : APIC_CMD_DEST_MODE_NMI;

    uint32_t dest = (target_apic_id >= 0) ? target_apic_id : 0;

    command(dest, APIC_CMD_VECTOR_n(intr) | dest_type | dest_mode);
}

uint32_t lapic_t::timer_count()
{
    return read32(APIC_REG_LVT_CCR);
}

void lapic_t::apic_calibrate()
{
    if (acpi_pm_timer_raw() >= 0) {
        //
        // Have PM timer

        // Program timer (should be high enough to measure 858ms @ 5GHz)
        apic->configure_timer(0, 0xFFFFFFF0U,
                              lapic_t::lvt_tr_mode_t::ONESHOT,
                              INTR_APIC_TIMER, true);

        uint32_t tmr_st = acpi_pm_timer_raw();
        uint32_t ccr_st = apic->timer_count();
        uint32_t tmr_en;
        uint64_t tsc_st = cpu_rdtsc();
        uint32_t tmr_diff;

        // Wait for about 1ms
        do {
            pause();
            tmr_en = acpi_pm_timer_raw();
            tmr_diff = acpi_pm_timer_diff(tmr_st, tmr_en);
        } while (tmr_diff < 3579);

        uint32_t ccr_en = apic->timer_count();
        uint64_t tsc_en = cpu_rdtsc();

        uint64_t tsc_elap = tsc_en - tsc_st;
        uint32_t ccr_elap = ccr_st - ccr_en;
        uint64_t tmr_nsec = acpi_pm_timer_ns(tmr_diff);

        uint64_t cpu_freq = (uint64_t(tsc_elap) * 1000000000) / tmr_nsec;
        uint64_t ccr_freq = (uint64_t(ccr_elap) * 1000000000) / tmr_nsec;

        smp.apic_timer_freq = ccr_freq;

        // Round CPU frequency to nearest multiple of 16MHz
        cpu_freq += 8333333;
        cpu_freq -= cpu_freq % 16666666;

        // Round APIC frequency to nearest multiple of 100MHz
        smp.apic_timer_freq += 50000000;
        smp.apic_timer_freq -= smp.apic_timer_freq % 100000000;

        smp.rdtsc_mhz = (cpu_freq + 500000) / 1000000;
    } else {
        // Program timer (should be high enough to measure 858ms @ 5GHz)
        apic->configure_timer(0, 0xFFFFFFF0U,
                              lapic_t::lvt_tr_mode_t::ONESHOT,
                              INTR_APIC_TIMER, true);

        uint32_t ccr_st = apic->timer_count();
        uint64_t tsc_st = cpu_rdtsc();

        // Wait for about 1ms
        uint64_t tmr_nsec = nsleep(1000000);

        uint32_t ccr_en = apic->timer_count();
        uint64_t tsc_en = cpu_rdtsc();

        uint64_t tsc_elap = tsc_en - tsc_st;
        uint32_t ccr_elap = ccr_st - ccr_en;

        uint64_t cpu_freq = (uint64_t(tsc_elap) * 1000000000) / tmr_nsec;
        uint64_t ccr_freq = (uint64_t(ccr_elap) * 1000000000) / tmr_nsec;

        smp.apic_timer_freq = ccr_freq;
        smp.rdtsc_mhz = (cpu_freq + 500000) / 1000000;
    }

    // Example: let rdtsc_mhz = 2500. gcd(1000,2500) = 500
    // then,
    //  clk_to_ns_numer = 1000/500 = 2
    //  chk_to_ns_denom = 2500/500 = 5
    // clk_to_ns: let clks = 2500000000
    //  2500000000 * 2 / 5 = 1000000000ns

    SMP_TRACE("CPU MHz: %ld\n", smp.rdtsc_mhz);

    uint64_t clk_to_ns_gcd = gcd(uint64_t(1000), smp.rdtsc_mhz);

    SMP_TRACE("CPU MHz GCD: %ld\n", clk_to_ns_gcd);

    smp.clk_to_ns_numer = 1000 / clk_to_ns_gcd;
    smp.clk_to_ns_denom = smp.rdtsc_mhz / clk_to_ns_gcd;

    SMP_TRACE("clk_to_ns_numer: %ld\n", smp.clk_to_ns_numer);
    SMP_TRACE("clk_to_ns_denom: %ld\n", smp.clk_to_ns_denom);


    if (cpuid_has_inrdtsc()) {
        SMP_TRACE("Using RDTSC for precision timing\n");
        time_ns_set_handler(apic_rdtsc_time_ns_handler, nullptr, true);
    }

    SMP_TRACE("CPU clock: %luMHz\n", smp.rdtsc_mhz);
    SMP_TRACE("APIC clock: %luHz\n", smp.apic_timer_freq);
}

void lapic_t::dump_regs(int ap)
{
#if DEBUG_APIC
    for (int i = 0; i < 64; i += 4) {
        printdbg("ap=%d APIC: ", ap);
        for (int x = 0; x < 4; ++x) {
            if (apic->reg_readable(i + x)) {
                printdbg("%s[%3x]=%08x%s",
                         x == 0 ? "apic: " : "",
                         (i + x),
                         apic->read32(i + x),
                         x == 3 ? "\n" : " ");
            } else {
                printdbg("%s[%3x]=--------%s",
                         x == 0 ? "apic: " : "",
                         i + x,
                         x == 3 ? "\n" : " ");
            }
        }
    }
    APIC_TRACE("Logical destination register value: 0x%x\n",
               apic->read32(APIC_REG_LDR));
#endif
}

isr_context_t *lapic_t::apic_timer_handler(int intr, isr_context_t *ctx)
{
    apic->eoi(intr);
    return thread_schedule(ctx);
}

isr_context_t *lapic_t::apic_spurious_handler(int intr, isr_context_t *ctx)
{
    (void)intr;
    assert(intr == INTR_APIC_SPURIOUS);
    APIC_ERROR("Spurious APIC interrupt!\n");
    return ctx;
}

unsigned lapic_t::get_id()
{
    if (likely(apic))
        return apic->read32(APIC_REG_ID);

    cpuid_t cpuid_info;
    cpuid(&cpuid_info, CPUID_INFO_FEATURES, 0);
    unsigned apic_id = cpuid_info.ebx >> 24;
    return apic_id;
}

uint32_t lapic_t::configure_timer(uint32_t initial_count,
                                  lvt_tr_mode_t timer_mode,
                                  uint8_t intr, uint8_t log2_divide_by,
                                  bool mask)
{
    static uint32_t dcr_lookup[] = {
        APIC_LVT_DCR_BY_1,
        APIC_LVT_DCR_BY_2,
        APIC_LVT_DCR_BY_4,
        APIC_LVT_DCR_BY_8,
        APIC_LVT_DCR_BY_16,
        APIC_LVT_DCR_BY_32,
        APIC_LVT_DCR_BY_64,
        APIC_LVT_DCR_BY_128
    };

    assert(log2_divide_by < countof(dcr_lookup));

    APIC_TRACE("configuring timer,"
               " dcr=0x%x, icr=0x%x, mode=0x%x, intr=0x%x, mask=%d\n",
               dcr, icr, timer_mode, intr, mask);
    write32(APIC_REG_LVT_DCR, dcr_lookup[log2_divide_by]);
    write32(APIC_REG_LVT_TR, APIC_LVT_VECTOR_n(intr) |
            APIC_LVT_TR_MODE_n(timer_mode) |
            (mask ? APIC_LVT_MASK : 0));
    write32(APIC_REG_LVT_ICR, initial_count);

    return initial_count;
}

uint32_t lapic_t::configure_timer(uint64_t initial_count,
                              lapic_t::lvt_tr_mode_t timer_mode,
                              uint8_t intr, bool mask)
{
    // Maximize divisor without losing precision, and
    // enforce that the initial count fits in a 32 bit value
    uint8_t log2_divide_by = 0;
    while (log2_divide_by < 7 &&
           (((initial_count & 1) == 1) || (initial_count > 0xFFFFFFFF)))
    {
        initial_count >>= 1;
        ++log2_divide_by;
    }

    return configure_timer(initial_count, timer_mode,
                           intr, mask, log2_divide_by);
}

bool lapic_t::reg_maybe_exists(uint32_t reg)
{
    // Reserved registers:
    return  !(reg >= 0x40) &&
            !(reg <= 0x01) &&
            !(reg >= 0x04 && reg <= 0x07) &&
            !(reg == 0x0C) &&
            !(reg >= 0x29 && reg <= 0x2E) &&
            !(reg >= 0x3A && reg <= 0x3D) &&
            !(reg == 0x3F);
}

bool lapic_t::reg_maybe_readable(uint32_t reg)
{
    return reg != APIC_REG_EOI &&
            reg != APIC_REG_SELF_IPI &&
            reg_maybe_exists(reg);
}
