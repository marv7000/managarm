#pragma once

#include <stdint.h>

namespace eir {

using address_t = uint64_t;
using physaddr_t = uint64_t;

extern uintptr_t eirTTBR[2];

extern "C" [[noreturn]] void
eirEnterKernel(uintptr_t ttbr0, uintptr_t ttbr1, uint64_t entry, uint64_t stack);

} // namespace eir
