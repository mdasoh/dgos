#pragma once
#include "types.h"
//#include "cpu/isr.h"

struct isr_context_t;

// Located nearby data touched in interrupt handling
extern size_t sse_context_size;
extern uint64_t sse_xsave_mask;

extern uint16_t sse_avx_offset;
extern uint16_t sse_avx_size;
extern uint16_t sse_avx512_opmask_offset;
extern uint16_t sse_avx512_opmask_size;
extern uint16_t sse_avx512_upper_offset;
extern uint16_t sse_avx512_upper_size;
extern uint16_t sse_avx512_xregs_offset;
extern uint16_t sse_avx512_xregs_size;

// These point to immediately after the operand of a jmp instruction
extern uint32_t * const sse_context_save;
extern uint32_t * const sse_context_restore;

// Keep this flag right next to other data touched in interrupt handler
extern bool cpu_has_fsgsbase;

// MSI IRQ
struct msi_irq_mem_t {
    uintptr_t addr;
    uintptr_t data;
};

typedef isr_context_t *(*intr_handler_t)(int intr, isr_context_t*);

typedef void (*irq_setmask_handler_t)(int irq, bool unmask);
typedef void (*irq_hook_handler_t)(int irq, intr_handler_t handler,
                                   char const * name);
typedef void (*irq_unhook_handler_t)(int irq, intr_handler_t handler);
typedef int (*msi_irq_alloc_handler_t)(
        msi_irq_mem_t *results, int count,
        int cpu, int distribute);
typedef bool (*irq_setcpu_handler_t)(int irq, int cpu);

//
// Vectors set by interrupt controller implementation

// Change irq mask
extern "C" void irq_setmask_set_handler(irq_setmask_handler_t handler);

// Set the appropriate interrupt vector for the specified irq
extern "C" void irq_hook_set_handler(irq_hook_handler_t handler);

// Reset the appropriate interrupt vector for the specified irq
extern "C" void irq_unhook_set_handler(irq_unhook_handler_t handler);

// Allocate MSI IRQ vector(s) and return address and data to use
extern "C" void msi_irq_alloc_set_handler(msi_irq_alloc_handler_t handler);

// Route the specified IRQ to the specified CPU
extern "C" void irq_setcpu_set_handler(irq_setcpu_handler_t handler);

// Change irq mask
extern "C" void irq_setmask(int irq, bool unmask);

// Set the appropriate interrupt vector for the specified irq
extern "C" void irq_hook(int irq, intr_handler_t handler, char const *name);

// Reset the appropriate interrupt vector for the specified irq
extern "C" void irq_unhook(int irq, intr_handler_t handler);

//
// Interrupt vector manipulation and dispatch

// Set interrupt vector
extern "C" void intr_hook(int intr, intr_handler_t handler, char const *name);

// Reset interrupt vector
extern "C" void intr_unhook(int intr, intr_handler_t handler);

// Call the interrupt handler
extern "C" isr_context_t *intr_invoke(int intr, isr_context_t *ctx);

// Call the appropriate interrupt vector for the specified irq
isr_context_t *irq_invoke(int intr, int irq, isr_context_t *ctx);

// Returns true if there is an interrupt handler for the interrupt
extern "C" int intr_has_handler(int intr);

int msi_irq_alloc(msi_irq_mem_t *results, int count,
                  int cpu, int distribute);

bool irq_setcpu(int irq, int cpu);
