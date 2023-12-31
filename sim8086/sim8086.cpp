
#include <fstream>
#include <iostream>
#include <vector>
#include <assert.h>
#include <limits>

#include "shared/sim86_shared.h"
#include "text_utils.h"

// Register mapping from Muratori's implementation.
enum RegisterType : u8
{
	Register_none = 0,

	Register_a,
	Register_b,
	Register_c,
	Register_d,
	Register_sp,
	Register_bp,
	Register_si,
	Register_di,
	Register_es,
	Register_cs,
	Register_ss,
	Register_ds,
	Register_ip,
	Register_flags,

	Register_count,
};

u16 registers[Register_count];
bool registerPrintFlags[Register_count];

const u8 CF = 1;
const u8 PF = 1 << 1;
const u8 AF = 1 << 2;
const u8 ZF = 1 << 3;
const u8 SF = 1 << 4;
const u8 OF = 1 << 5;

u8* simMemory;
const u32 memSize = 64 * 1024;

struct CycleEstimation
{
	u32 directCycle = 0;
	u32 eaCycle = 0;
	u32 penalty = 0;
};

constexpr u32 ToU32(const s16 value)
{
	return static_cast<u32>(static_cast<u16>(value));
}

enum class BitSelection : u8
{
	Lower,
	Upper,
};

inline u8 GetBits(const u16 val, const BitSelection selection)
{
	return (selection == BitSelection::Lower) ? static_cast<u8>(val) : static_cast<u8>(val >> 8);
}

/**
 * @brief Returned structure from execution calls that stores info about modified registers.
 */
struct ExecutionInfo
{
	RegisterType modifiedRegister = Register_none;
	bool flagModified = false;
};

void SetBitFlag(u16& bitflags, const u8 flag, const bool set)
{
	bitflags = set ? (bitflags | flag) : (bitflags & ~flag);
}

bool CheckOverflow(const s16 val1, const s16 val2, const s16 result)
{
	return ((val1 > 0) && (val2 > 0) && (result < 0)) || ((val1 < 0) && (val2 < 0) && (result > 0));
}

bool CheckParity(const u32 val)
{
	u8 count = static_cast<u8>(val);
	count = (val & 0x55) + ((val >> 1) & 0x55);
	count = (count & 0x33) + ((count >> 2) & 0x33);
	count = (count & 0x0F) + ((count >> 4) & 0x0F);
	return (count % 2) == 0;
}

u16 GetAddress(const effective_address_expression& expression)
{
	assert(ArrayCount(expression.Terms) <= 2);
	u16 address = static_cast<u16>(expression.Displacement);

	for (size_t idx = 0; idx < ArrayCount(expression.Terms); ++idx) {
		const RegisterType regType = static_cast<RegisterType>(expression.Terms[idx].Register.Index);
		if (regType != Register_none) {
			address += registers[regType];
		}
	}

	return address;
}

void WriteToAddress(const u16 address, const u16 val, const bool wide)
{
	if (!wide) {
		simMemory[address] = GetBits(val, BitSelection::Lower);
		return;
	}

	simMemory[address] = GetBits(val, BitSelection::Lower);
	simMemory[address + 1] = GetBits(val, BitSelection::Upper);
}

u16 GetOperandValue(const instruction_operand operand, const bool wide)
{
	switch (operand.Type) {
		case Operand_Immediate:
			return operand.Immediate.Value & 0xFFFF;
		case Operand_Register: {
			const register_access reg = operand.Register;
			assert(reg.Count == static_cast<u32>(wide) + 1);

			const u16 regVal = registers[reg.Index];
			return wide ? regVal : GetBits(regVal, static_cast<BitSelection>(reg.Offset & 1));
		}
		case Operand_Memory: {
			const u16 address = GetAddress(operand.Address);
			u16 val = simMemory[address];
			if (wide) {
				val += static_cast<u16>(simMemory[address + 1]) << 8;
			}
			return val;
		}
		case Operand_None:
			printf("Invalid operand type\n");
			return 0;
	}
}

ExecutionInfo ExecuteMov(const instruction& decodedInstruction)
{
	assert(decodedInstruction.Op == Op_mov);

	ExecutionInfo execInfo;
	const bool wide = decodedInstruction.Flags & Inst_Wide;
	const u16 sourceVal = GetOperandValue(decodedInstruction.Operands[1], wide);
	const instruction_operand target = decodedInstruction.Operands[0];

	switch (target.Type) {
		case Operand_Register: {
			const register_access targetReg = target.Register;
			assert(targetReg.Count == static_cast<u32>(wide) + 1);
			u16& targetVal = registers[targetReg.Index];

			if (wide) {
				targetVal = sourceVal;
			}
			else {
				const bool high = targetReg.Offset & 1;
				targetVal = high ?
							GetBits(targetVal, BitSelection::Lower) | (sourceVal << 8) :
							GetBits(targetVal, BitSelection::Upper) | sourceVal;
			}

			execInfo.modifiedRegister = static_cast<RegisterType>(targetReg.Index);
			break;
		}
		case Operand_Memory: {
			const u16 address = GetAddress(target.Address);
			WriteToAddress(address, sourceVal, wide);
			break;
		}
		default:
			printf("Unimplemented operand type\n");
	}

	return execInfo;
}

ExecutionInfo ExecuteArithmetic(const instruction& decodedInstruction)
{
	ExecutionInfo executionInfo;
	const bool wide = decodedInstruction.Flags & Inst_Wide;

	const instruction_operand firstOperand = decodedInstruction.Operands[0];
	const instruction_operand secondOperand = decodedInstruction.Operands[1];
	const s16 targetVal = static_cast<s16>(GetOperandValue(firstOperand, wide));
	const s16 sourceVal = static_cast<s16>(GetOperandValue(secondOperand, wide));

	bool overflow = false;
	bool auxCarry = false;
	bool skipCF = false;

	bool writesResult = false;
	u32 result = 0;
	switch (decodedInstruction.Op) {
		case Op_add: {
			result = ToU32(targetVal) + ToU32(sourceVal);
			overflow = CheckOverflow(targetVal, sourceVal, static_cast<s16>(result));
			auxCarry = ((targetVal & 0x0F) + (sourceVal & 0x0F)) & 0x10;
			writesResult = true;
			break;
		}
		case Op_inc: {
			result = ToU32(targetVal) + u32(1);
			overflow = CheckOverflow(targetVal, sourceVal, static_cast<s16>(result));
			auxCarry = ((targetVal & 0x0F) + (sourceVal & 0x0F)) & 0x10;
			skipCF = true;
			writesResult = true;
			break;
		}
		case Op_sub: {
			result = ToU32(targetVal) - ToU32(sourceVal);
			overflow = CheckOverflow(targetVal, -sourceVal, static_cast<s16>(result));
			auxCarry = ((targetVal & 0x0F) - (sourceVal & 0x0F)) & 0x10;
			writesResult = true;
			break;
		}
		case Op_cmp:
			result = ToU32(targetVal) - ToU32(sourceVal);
			overflow = CheckOverflow(targetVal, -sourceVal, static_cast<s16>(result));
			auxCarry = ((targetVal & 0x0F) - (sourceVal & 0x0F)) & 0x10;
			break;

		default:
			break;
	}

	if (writesResult) {
		switch (firstOperand.Type) {
			case Operand_Register:
				registers[firstOperand.Register.Index] = static_cast<u16>(result & 0x0000FFFF);
				executionInfo.modifiedRegister = static_cast<RegisterType>(firstOperand.Register.Index);
				break;
			case Operand_Memory: {
				const u16 address = GetAddress(firstOperand.Address);
				WriteToAddress(address, sourceVal, wide);
				break;
			}
			default:
				printf("Invalid operand type\n");
				break;
		}
	}

	u16& flags = registers[Register_flags];
	SetBitFlag(flags, ZF, (static_cast<u16>(result) == 0));
	SetBitFlag(flags, SF, (result & 0x8000));
	SetBitFlag(flags, OF, overflow);
	SetBitFlag(flags, AF, auxCarry);
	SetBitFlag(flags, PF, CheckParity(result));
	if (!skipCF) {
		SetBitFlag(flags, CF, (result & 0x00010000));
	}
	executionInfo.flagModified = true;

	return executionInfo;
}

ExecutionInfo ExecuteJump(const instruction& decodedInstruction)
{
	ExecutionInfo executionInfo;

	bool jump = false;

	switch (decodedInstruction.Op) {
		case Op_je:
			jump = (registers[Register_flags] & ZF);
			break;
		case Op_jne:
			jump = !(registers[Register_flags] & ZF);
			break;
		case Op_jp:
			jump = (registers[Register_flags] & PF);
			break;
		case Op_jb:
			jump = (registers[Register_flags] & CF);
			break;
		case Op_loop:
			registers[Register_c]--;
			jump = (registers[Register_c] != 0);
			executionInfo.modifiedRegister = Register_c;
			break;
		case Op_loopnz:
			registers[Register_c]--;
			jump = (registers[Register_c] != 0) && (!(registers[Register_flags] & ZF));
			executionInfo.modifiedRegister = Register_c;
			break;
		default:
			break;
	}

	if (!jump) {
		return executionInfo;
	}

	const s16 jumpOffset = static_cast<s16>(decodedInstruction.Operands[0].Immediate.Value);
	const s16 ip = static_cast<s16>(registers[Register_ip]);
	registers[Register_ip] = static_cast<u16>(ip + jumpOffset);

	return executionInfo;
}

ExecutionInfo ExecuteLogical(const instruction& decodedInstruction)
{
	ExecutionInfo executionInfo;
	const bool wide = decodedInstruction.Flags & Inst_Wide;

	const instruction_operand firstOperand = decodedInstruction.Operands[0];
	const instruction_operand secondOperand = decodedInstruction.Operands[1];
	const s16 targetVal = static_cast<s16>(GetOperandValue(firstOperand, wide));
	const s16 sourceVal = static_cast<s16>(GetOperandValue(secondOperand, wide));

	bool writesResult = false;
	u16 result = 0;
	switch (decodedInstruction.Op) {
		case Op_xor:
			result = targetVal ^ sourceVal;
			writesResult = true;
			break;
	}

	if (writesResult) {
		switch (firstOperand.Type) {
		case Operand_Register:
			registers[firstOperand.Register.Index] = result & 0x0000FFFF;
			executionInfo.modifiedRegister = static_cast<RegisterType>(firstOperand.Register.Index);
			break;
		case Operand_Memory: {
			const u16 address = GetAddress(firstOperand.Address);
			WriteToAddress(address, sourceVal, wide);
			break;
		}
		default:
			printf("Invalid operand type\n");
			break;
		}
	}

	u16& flags = registers[Register_flags];
	SetBitFlag(flags, ZF, (result == 0));
	SetBitFlag(flags, SF, (result & 0x8000));
	SetBitFlag(flags, OF, false);
	SetBitFlag(flags, AF, false);
	SetBitFlag(flags, CF, false);
	SetBitFlag(flags, PF, CheckParity(result));
	executionInfo.flagModified = true;

	return executionInfo;
}

ExecutionInfo ExecuteInstruction(const instruction& decodedInstruction)
{
	switch (decodedInstruction.Op) {
		case Op_mov:
			return ExecuteMov(decodedInstruction);
		case Op_xor:
			return ExecuteLogical(decodedInstruction);
		case Op_cmp:
		case Op_add:
		case Op_sub:
		case Op_inc:
			return ExecuteArithmetic(decodedInstruction);
		case Op_je:
		case Op_jne:
		case Op_jp:
		case Op_jb:
		case Op_loopnz:
		case Op_loop:
			return ExecuteJump(decodedInstruction);
		default:
			printf("Unimplemented operation type\n");
			break;
	}

	return ExecutionInfo();
}

std::string GetActiveFlagNames(const u16 bitFlags)
{
	static const std::string FLAG_NAMES[6] = { "C", "P", "A", "Z", "S", "O" };

	std::string result;
	for (size_t i = 0; i < ArrayCount(FLAG_NAMES); ++i) {
		if (bitFlags & (1 << i)) {
			result.append(FLAG_NAMES[i]);
		}
	}

	return result;
}

void PrintExecutionState(const ExecutionInfo modifyInfo, const u16 (&backupRegisters)[Register_count], FILE* Dest)
{
	fprintf(Dest, " ;");

	if (modifyInfo.modifiedRegister != Register_none) {
		registerPrintFlags[modifyInfo.modifiedRegister] = true;
		register_access reg = { modifyInfo.modifiedRegister, 0, 2 };
		fprintf(Dest, " %s:", Sim86_RegisterNameFromOperand(&reg));
		fprintf(Dest, "0x%x->0x%x", backupRegisters[reg.Index], registers[reg.Index]);
	}

	fprintf(Dest, " ip:0x%x->0x%x", backupRegisters[Register_ip], registers[Register_ip]);

	const u16 flags = registers[Register_flags];
	const u16 backupFlags = backupRegisters[Register_flags];
	if (modifyInfo.flagModified) {
		fprintf(Dest, " flags:%s->%s", GetActiveFlagNames(backupFlags).c_str(), GetActiveFlagNames(flags).c_str());
	}

	fprintf(Dest, "\n");
}

void PrintFinalState(FILE* Dest)
{
	fprintf(Dest, "Final Registers:\n");

	for (u16 i = 1; i < Register_flags; ++i) {
		if (!registerPrintFlags[i]) {
			continue;
		}

		register_access reg{ i, 0 , 2 };
		fprintf(Dest, "      %s: 0x%.4x (%d)\n", Sim86_RegisterNameFromOperand(&reg), registers[i], registers[i]);
	}


	const u16 flags = registers[Register_flags];
	fprintf(Dest, "   flags: %s\n", GetActiveFlagNames(flags).c_str());
}

u32 ComputeEACycleCount(const effective_address_expression expression)
{
	const bool hasDisplacement = expression.Displacement != 0;
	RegisterType baseReg = Register_none;
	RegisterType idxReg = Register_none;

	for (size_t idx = 0; idx < ArrayCount(expression.Terms); ++idx) {
		const RegisterType regType = static_cast<RegisterType>(expression.Terms[idx].Register.Index);
		if ((regType == Register_b) || (regType == Register_bp)) {
			baseReg = regType;
		}
		else if ((regType == Register_si) || (regType == Register_di)) {
			idxReg = regType;
		}
	}

	if (!baseReg && !idxReg) {
		assert(hasDisplacement);
		return 6;
	}

	u32 cycleCount = 0;

	if (!baseReg ^ !idxReg) {
		cycleCount = 5;
	}
	else if ((baseReg == Register_bp && idxReg == Register_di) || (baseReg == Register_b && idxReg == Register_si)) {
		cycleCount = 7;
	}
	else {
		cycleCount = 8;
	}

	if (hasDisplacement) {
		cycleCount += 4;
	}

	return cycleCount;
}

CycleEstimation EstimateCycle(const instruction& decodedInstruction)
{
	CycleEstimation result;

	const instruction_operand operand0 = decodedInstruction.Operands[0];
	const instruction_operand operand1 = decodedInstruction.Operands[1];
	const operand_type type0 = operand0.Type;
	const operand_type type1 = operand1.Type;

	const bool acc = ((type0 == Operand_Register) && (operand0.Register.Index == Register_a))
					|| ((type1 == Operand_Register) && (operand1.Register.Index == Register_a));

	u32 penaltyCoeff = 0;

	switch (decodedInstruction.Op) {
		case Op_mov: {
			if (type0 == Operand_Register) {
				if (type1 == Operand_Register) {
					result.directCycle = 2;
				}
				else if (type1 == Operand_Immediate) {
					result.directCycle = 4;
				}
				else if (type1 == Operand_Memory) {
					result.directCycle = acc ? 10 : 8;
					result.eaCycle = acc ? 0 : ComputeEACycleCount(operand1.Address);
					result.penalty = (GetAddress(operand1.Address) % 2) * 4;
				}
			}
			else if (type0 == Operand_Memory) {
				result.penalty = (GetAddress(operand1.Address) % 2) * 4;
				result.eaCycle = acc ? 0 : ComputeEACycleCount(operand0.Address);
				if (type1 == Operand_Register) {
					result.directCycle = acc ? 10 : 9;
				}
				else if (type1 == Operand_Immediate) {
					result.directCycle = 10;
				}
			}
			break;
		}
		case Op_sub:
		case Op_add: {
			if (type0 == Operand_Register) {
				if (type1 == Operand_Register) {
					result.directCycle = 3;
				}
				else if (type1 == Operand_Immediate) {
					result.directCycle = 4;
				}
				else if (type1 == Operand_Memory) {
					const u32 penalty = (GetAddress(operand1.Address) % 2) * 4;
					result = { 9, ComputeEACycleCount(operand1.Address), penalty};
				}
			}
			else if (type0 == Operand_Memory) {
				result.penalty = (GetAddress(operand0.Address) % 2) * 2 * 4;
				result.eaCycle = ComputeEACycleCount(operand0.Address);
				result.directCycle = (type1 == Operand_Register) ? 16 : 17;
			}

			break;
		}
		default:
			printf("Unimplemented operation type\n");
			break;
	}

	return result;
}

void Execute8086(std::vector<u8>& buffer)
{
	const u32 instructionSize = static_cast<u32>(buffer.size());
	buffer.resize(memSize);
	simMemory = buffer.data();

	u16& ip = registers[Register_ip];

	while (ip < instructionSize) {
		instruction decoded;
		Sim86_Decode8086Instruction(instructionSize - ip, &simMemory[ip], &decoded);

		if (!decoded.Op) {
			printf("Unrecognized instruction.\n");
			break;
		}

		if (decoded.Op == Op_ret || decoded.Op == Op_retf) {
			printf("Return instruction.\n");
			break;
		}

		u16 registersBeforeExecution[Register_count];
		std::memcpy(registersBeforeExecution, registers, Register_count * sizeof(u16));

		ip += static_cast<u16>(decoded.Size);
		registerPrintFlags[Register_ip] = true;

		PrintInstruction(decoded, stdout);
		const ExecutionInfo modifications = ExecuteInstruction(decoded);
		PrintExecutionState(modifications, registersBeforeExecution, stdout);
	}

	PrintFinalState(stdout);
}

void Disassemble8086(std::vector<u8>& buffer)
{
	printf("bits 16\n");

	u8* instructions = &buffer[0];
	const u32 bufferSize = static_cast<u32>(buffer.size());

	u32 offset = 0;
	while (offset < bufferSize) {
		instruction decoded;
		Sim86_Decode8086Instruction(bufferSize - offset, instructions + offset, &decoded);
		if (decoded.Op) {
			offset += decoded.Size;
			PrintInstruction(decoded, stdout);
			printf("\n");
		}
		else {
			printf("Unrecognized instruction\n");
			break;
		}
	}
}

void DumpMemoryToFile(const std::string& fileName)
{
	std::basic_ofstream<u8> stream{ fileName, stream.binary};
	if (!stream.is_open()) {
		std::cout << "Failed to open!" << std::endl;
		return;
	}

	stream.write(simMemory, memSize);
}

void EstimateClocks(std::vector<u8>& buffer)
{
	u8* instructions = &buffer[0];
	const u32 bufferSize = static_cast<u32>(buffer.size());

	u32 totalCycleCount = 0;

	u32 offset = 0;
	while (offset < bufferSize) {
		instruction decoded;
		Sim86_Decode8086Instruction(bufferSize - offset, instructions + offset, &decoded);
		if (! decoded.Op) {
			printf("Unrecognized instruction\n");
			break;
		}

		offset += decoded.Size;
		PrintInstruction(decoded, stdout);

		fprintf(stdout, " ;");

		const CycleEstimation cycleCount = EstimateCycle(decoded);
		const u32 instructionCycleCount = cycleCount.directCycle + cycleCount.eaCycle + cycleCount.penalty;
		totalCycleCount += instructionCycleCount;
		fprintf(stdout, " Clocks: +%d = %d", instructionCycleCount, totalCycleCount);

		std::string detailedCycleInfo;
		if (cycleCount.eaCycle) {
			fprintf(stdout, " (%d + %dea", cycleCount.directCycle, cycleCount.eaCycle);

			if (cycleCount.penalty) {
				fprintf(stdout, " + %dp", cycleCount.penalty);
			}

			fprintf(stdout, ")");
		}

		fprintf(stdout, "\n");
	}

}

int main(int argc, char* argv[])
{
	if (argc < 3) {
		fprintf(stderr, "USAGE: %s [8086 machine code file] ...\n", argv[0]);
		return -1;
	}

	u32 Version = Sim86_GetVersion();
	printf("Sim86 Version: %u (expected %u)\n", Version, SIM86_VERSION);
	if (Version != SIM86_VERSION) {
		printf("ERROR: Header file version doesn't match DLL.\n");
		return -1;
	}

	char* fileName = argv[1];
	char* runMode = argv[2];
	const bool dump = (argc == 4) && (strcmp(argv[3], "-dump") == 0);

	std::basic_ifstream<u8> stream{ fileName, stream.binary};
	if (!stream.is_open()) {
		std::cout << "Failed to open!" << std::endl;
		return -1;
	}

	std::vector<u8> buffer((std::istreambuf_iterator<u8>(stream)), std::istreambuf_iterator<u8>());

	if (strcmp(runMode, "-disasm") == 0) {
		Disassemble8086(buffer);
	}
	else if (strcmp(runMode, "-exec") == 0) {
		Execute8086(buffer);

		if (dump) {
			DumpMemoryToFile("sim86_memory_0.data");
		}
	}
	else if (strcmp(runMode, "-showclocks") == 0) {
		EstimateClocks(buffer);
	}

	return 0;
}
