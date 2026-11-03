/**
 * This submodule contains process related helper functions 
 * and factory methods for the process instructions
 * 
 * This code was only modularized after ya'll made the process.cpp a god class ;-;
 * - Stephy
*/

#include <process/Instruction.hpp>

Operand Operand::fromVariable(std::string name)
{
    Operand op;
    op.isVariable = true;
    op.varName = std::move(name);
    return op;
}

Operand Operand::fromValue(uint16_t val)
{
    Operand op;
    op.isVariable = false;
    op.literalValue = val;
    return op;
}

Instruction Instruction::makePrint(std::string msg, std::string var)
{
    Instruction inst;
    inst.type = InstructionType::Print;
    inst.message = std::move(msg);
    inst.printVar = std::move(var);
    return inst;
}

Instruction Instruction::makeDeclare(std::string var, uint16_t val)
{
    Instruction inst;
    inst.type = InstructionType::Declare;
    inst.declareVar = std::move(var);
    inst.declareValue = val;
    return inst;
}

Instruction Instruction::makeAdd(std::string dest, Operand op1, Operand op2)
{
    Instruction inst;
    inst.type = InstructionType::Add;
    inst.destVar = std::move(dest);
    inst.op1 = std::move(op1);
    inst.op2 = std::move(op2);
    return inst;
}

Instruction Instruction::makeSubtract(std::string dest, Operand op1, Operand op2)
{
    Instruction inst;
    inst.type = InstructionType::Subtract;
    inst.destVar = std::move(dest);
    inst.op1 = std::move(op1);
    inst.op2 = std::move(op2);
    return inst;
}

Instruction Instruction::makeSleep(uint8_t ticks)
{
    Instruction inst;
    inst.type = InstructionType::Sleep;
    inst.sleepTicks = ticks;
    return inst;
}

Instruction Instruction::makeFor(std::vector<Instruction> body, uint32_t repeats)
{
    Instruction inst;
    inst.type = InstructionType::For;
    inst.nestedInstructions = std::move(body);
    inst.repeats = repeats;
    return inst;
}

Instruction Instruction::makeInvalid()
{
    Instruction inst;
    inst.type = InstructionType::Invalid;
    return inst;
}