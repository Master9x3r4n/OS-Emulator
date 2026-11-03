#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <process/Instruction.hpp>

namespace InstructionGenerator {

inline constexpr size_t kMaxLoopDepth = 3;

std::vector<Instruction> generateRandomInstructions(
    uint64_t minIns, uint64_t maxIns, const std::string& procName, uint32_t currentDepth = 0);

uint32_t countTotalInstructions(const std::vector<Instruction>& insts, size_t currentDepth = 0);

} // namespace InstructionGenerator