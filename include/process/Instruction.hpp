#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

class Process;

enum class InstructionType {
    Print,
    Declare,
    Add,
    Subtract,
    Sleep,
    For,
    Invalid
};

struct Operand {
    bool isVariable = false;
    std::string varName;
    uint16_t literalValue = 0;

    Operand() = default;

    static Operand fromVariable(std::string name);
    static Operand fromValue(uint16_t val);
};

struct Instruction {
    InstructionType type = InstructionType::Print;

    std::string message;
    std::string printVar;

    std::string declareVar;
    uint16_t declareValue = 0;

    std::string destVar;
    Operand op1;
    Operand op2;

    uint8_t sleepTicks = 0;

    uint32_t repeats = 1;
    std::vector<Instruction> nestedInstructions;

    static Instruction makePrint(std::string msg = "", std::string var = "");
    static Instruction makeDeclare(std::string var, uint16_t val);
    static Instruction makeAdd(std::string dest, Operand op1, Operand op2);
    static Instruction makeSubtract(std::string dest, Operand op1, Operand op2);
    static Instruction makeSleep(uint8_t ticks);
    static Instruction makeFor(std::vector<Instruction> body, uint32_t repeats);
    static Instruction makeInvalid();
};

struct DecodedInstruction {
    InstructionType type = InstructionType::Invalid;
    bool isValid = true;
    std::string printOutput;
    std::string targetVar;
    uint16_t computedValue = 0;
    uint8_t sleepTicks = 0;
    const std::vector<Instruction>* forBody = nullptr;
    uint32_t forRepeats = 0;
};

struct ExecutionContext {
    Process& process;
    uint32_t core;
    std::chrono::system_clock::time_point timestamp;
};