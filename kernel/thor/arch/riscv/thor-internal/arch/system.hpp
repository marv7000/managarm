#pragma once

#include <eir/interface.hpp>
#include <thor-internal/elf-notes.hpp>

namespace thor {

// TODO: Remove IRQ slots entirely on RISC-V.
static inline constexpr int numIrqSlots = 0;

extern ManagarmElfNote<RiscvConfig> riscvConfigNote;
extern ManagarmElfNote<RiscvHartCaps> riscvHartCapsNote;

void initializeArchitecture();

} // namespace thor
