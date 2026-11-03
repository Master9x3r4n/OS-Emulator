/**
 * This submodule contains the code for the random instruction
 * generation.
 * 
 * This code was only modularized after ya'll made the process.cpp a god class ;-;
 * - Stephy
*/

#include <process/InstructionGenerator.hpp>

#include <algorithm>
#include <random>

namespace InstructionGenerator {

std::vector<Instruction> generateRandomInstructions(
    uint64_t minIns, uint64_t maxIns, const std::string& procName, uint32_t currentDepth)
{
    static thread_local std::mt19937_64 rng(std::random_device{}());

    const uint64_t low = std::max<uint64_t>(1ULL, minIns);
    const uint64_t high = std::max<uint64_t>(low, maxIns);
    std::uniform_int_distribution<unsigned long long> countDist(low, high);
    const uint64_t targetCount = countDist(rng);

    const std::vector<std::string> varPool = {"x", "y", "z", "count"};
    std::uniform_int_distribution<int> varDist(0, static_cast<int>(varPool.size() - 1));
    std::uniform_int_distribution<uint16_t> valDist(1, 100);
    std::uniform_int_distribution<int> boolDist(0, 1);
    std::uniform_int_distribution<int> sleepDist(1, 5);

    std::vector<Instruction> result;
    uint64_t remaining = targetCount;

    // At top level, seed with an initial declaration so variables exist
    if (currentDepth == 0 && remaining > 0)
    {
        result.push_back(Instruction::makeDeclare(varPool[0], valDist(rng)));
        --remaining;
    }

    while (remaining > 0)
    {
        const bool canMakeFor = (currentDepth < kMaxLoopDepth) && (remaining >= 2);
        std::uniform_int_distribution<int> typeDist(0, canMakeFor ? 5 : 4);
        const int choice = typeDist(rng);

        switch (choice)
        {
        case 0: { // PRINT
            if (boolDist(rng) == 0)
            {
                result.push_back(Instruction::makePrint("Hello world from " + procName + "!"));
            }
            else
            {
                const std::string& v = varPool[varDist(rng)];
                result.push_back(Instruction::makePrint("Value of " + v + ": ", v));
            }
            --remaining;
            break;
        }
        case 1: { // DECLARE
            const std::string& v = varPool[varDist(rng)];
            result.push_back(Instruction::makeDeclare(v, valDist(rng)));
            --remaining;
            break;
        }
        case 2: { // ADD
            const std::string& dest = varPool[varDist(rng)];
            Operand op1 = (boolDist(rng) == 0) ? Operand::fromVariable(varPool[varDist(rng)])
                                               : Operand::fromValue(valDist(rng));
            Operand op2 = (boolDist(rng) == 0) ? Operand::fromVariable(varPool[varDist(rng)])
                                               : Operand::fromValue(valDist(rng));
            result.push_back(Instruction::makeAdd(dest, op1, op2));
            --remaining;
            break;
        }
        case 3: { // SUBTRACT
            const std::string& dest = varPool[varDist(rng)];
            Operand op1 = (boolDist(rng) == 0) ? Operand::fromVariable(varPool[varDist(rng)])
                                               : Operand::fromValue(valDist(rng));
            Operand op2 = (boolDist(rng) == 0) ? Operand::fromVariable(varPool[varDist(rng)])
                                               : Operand::fromValue(valDist(rng));
            result.push_back(Instruction::makeSubtract(dest, op1, op2));
            --remaining;
            break;
        }
        case 4: { // SLEEP
            result.push_back(Instruction::makeSleep(static_cast<uint8_t>(sleepDist(rng))));
            --remaining;
            break;
        }
        case 5: { // FOR
            const uint64_t maxRepeats = std::min<uint64_t>(3ULL, remaining);
            std::uniform_int_distribution<uint64_t> repeatDist(2ULL, maxRepeats);
            const uint64_t repeats = repeatDist(rng);

            const uint64_t maxAllowedBudget = remaining / repeats;
            const uint64_t maxBodyBudget = std::min<uint64_t>(8ULL, maxAllowedBudget);
            const uint64_t minBodyBudget = (maxBodyBudget >= 2) ? 2ULL : 1ULL;

            std::uniform_int_distribution<uint64_t> bodyBudgetDist(minBodyBudget, maxBodyBudget);
            const uint64_t bodyBudget = bodyBudgetDist(rng);

            std::vector<Instruction> loopBody = generateRandomInstructions(bodyBudget, bodyBudget, procName, currentDepth + 1);
            result.push_back(Instruction::makeFor(std::move(loopBody), static_cast<uint32_t>(repeats)));

            remaining -= (repeats * bodyBudget);
            break;
        }
        }
    }

    return result;
}

uint32_t countTotalInstructions(const std::vector<Instruction>& insts, size_t currentDepth)
{
    uint32_t total = 0;
    for (const auto& inst : insts)
    {
        if (inst.type == InstructionType::For)
        {
            if (currentDepth < kMaxLoopDepth && inst.repeats > 0)
            {
                uint32_t bodyCount = countTotalInstructions(inst.nestedInstructions, currentDepth + 1);
                total += bodyCount * inst.repeats;
            }
        }
        else
        {
            total += 1;
        }
    }
    return total;
}

} // namespace InstructionGenerator