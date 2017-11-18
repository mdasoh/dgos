#pragma once
#include "types.h"
#include "idt.h"
#include "apic.h"

class lapic_t {
public:
    enum struct lvt_tr_mode_t : uint8_t {
        ONESHOT = 0,
        PERIODIC = 1,
        DEADLINE = 2
    };

    static int apic_init(int ap);

    virtual void command(uint32_t dest, uint32_t cmd) const = 0;

    virtual uint32_t read32(uint32_t offset) const = 0;
    virtual void write32(uint32_t offset, uint32_t val) const = 0;

    virtual uint64_t read64(uint32_t offset) const = 0;
    virtual void write64(uint32_t offset, uint64_t val) const = 0;

    virtual bool reg_readable(uint32_t reg) const = 0;
    virtual bool reg_exists(uint32_t reg) const = 0;

    virtual void write_ldr(uint32_t val) const;

    void eoi(int intr);

    uint32_t configure_timer(
            uint32_t initial_count, lvt_tr_mode_t timer_mode,
            uint8_t intr, uint8_t log2_divide_by, bool mask = false);

    uint32_t configure_timer(
            uint64_t initial_count, lvt_tr_mode_t timer_mode,
            uint8_t intr, bool mask = false);

    void online(int enabled, int spurious_intr);

    void broadcast_init();

    void send_sipi(uint32_t target, uint32_t trampoline_page);

    void send_ipi(int target_apic_id, uint8_t intr);

    uint32_t timer_count();

    void calibrate();

    void dump_regs(int ap);

    static unsigned get_id(void);

    static isr_context_t *apic_timer_handler(int intr, isr_context_t *ctx);
    static isr_context_t *apic_spurious_handler(int intr, isr_context_t *ctx);

protected:
    static bool reg_maybe_exists(uint32_t reg);
    static bool reg_maybe_readable(uint32_t reg);

    static uintptr_t apic_base;
    static uint32_t volatile *apic_ptr;

private:
};

extern lapic_t *apic;
