#include "mp_tables.h"

char const * const intr_type_text[] = {
    "APIC",
    "NMI",
    "SMI",
    "EXTINT"
};

size_t intr_type_text_count = countof(intr_type_text);
