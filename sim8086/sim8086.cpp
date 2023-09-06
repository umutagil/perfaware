
#include <fstream>
#include <iostream>
#include <vector>
#include <assert.h>
#include <limits>

#include "shared/sim86_shared.h"
#include "text_utils.h"

class registerInfo
{
	public:
		inline s16 GetValue() const
		{
			return value;
		}

		inline void SetValue(const s16 newValue)
		{
			value = newValue;
			modified = true;
		}

		inline bool IsModified()
		{
			return modified;
		}

	private:
		s16 value = 0;
		bool modified = false;
};

registerInfo registers[13];

u8 flags = 0x00;
u8 CF = 0x01;
u8 PF = 0x02;
u8 AF = 0x04;
u8 ZF = 0x08;
u8 SF = 0x10;
u8 OF = 0x20;

std::string flagNames[6] = { "C", "P", "A", "Z", "S", "O" };

constexpr u32 ToU32(const s16 value)
{
	return static_cast<u32>(static_cast<u16>(value));
}

struct ExecutionContext
{
	register_access reg;
	s16 regValue;
	u8 flags;
};

struct OperationPrintDetails
{
	bool flagModifier = false;
	bool writesResult = true;
};

OperationPrintDetails GetOpPrintDetails(const operation_type opType)
{
	const bool flagModifier = true;
	const bool writesResult = true;

	switch (opType) {
		case Op_add:
		case Op_sub:
			return { flagModifier, writesResult };
		case Op_cmp:
			return { flagModifier, !writesResult };
		default:
			return OperationPrintDetails();
	}
}

void setBitFlag(u8& bitflags, const u8 flag, const bool set)
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

void ExecuteMov(const instruction& decodedInstruction)
{
	assert(decodedInstruction.Op == Op_mov);

	const bool wide = decodedInstruction.Flags & Inst_Wide;

	const register_access targetReg = decodedInstruction.Operands[0].Register;
	assert(targetReg.Count == static_cast<u32>(wide) + 1);

	instruction_operand secondOperand = decodedInstruction.Operands[1];
	switch (secondOperand.Type) {
		case Operand_Immediate: {
			const s16 immVal = secondOperand.Immediate.Value & 0xFFFF;
			if (wide) {
				registers[targetReg.Index].SetValue(immVal);
			}
			else {
				const s16 targetVal = registers[targetReg.Index].GetValue();
				const s16 source = immVal & 0xFF;
				const bool isTargetHigh = targetReg.Offset & 1;
				const s16 result = isTargetHigh ? (targetVal & 0x00FF) | (source << 8) : (targetVal & 0xFF00) | source;
				registers[targetReg.Index].SetValue(result);
			}

			break;
		}
		case Operand_Register: {
			const register_access sourceReg = decodedInstruction.Operands[1].Register;
			const s16 sourceVal = registers[sourceReg.Index].GetValue();

			if (wide) {
				registers[targetReg.Index].SetValue(sourceVal);
			}
			else {
				assert(sourceReg.Count == 1);

				const bool isTargetHigh = targetReg.Offset & 1;
				const bool isSourceHigh = sourceReg.Offset & 1;
				const s16 targetVal = registers[targetReg.Index].GetValue();
				const s16 source = isSourceHigh ? ((sourceVal & 0xFF00) >> 8) : (sourceVal & 0x00FF);
				const s16 result = isTargetHigh ? (targetVal & 0x00FF) | (source << 8) : (targetVal & 0xFF00) | source;
				registers[targetReg.Index].SetValue(result);
			}

			break;
		}
		default:
			printf("Unimplemented operand type\n");
	}
}

void ExecuteArithmetic(const instruction& decodedInstruction)
{
	assert((decodedInstruction.Op == Op_add) || (decodedInstruction.Op == Op_sub) || (decodedInstruction.Op == Op_cmp));

	const bool wide = decodedInstruction.Flags & Inst_Wide;

	const register_access targetReg = decodedInstruction.Operands[0].Register;
	const s16 targetVal = registers[targetReg.Index].GetValue();
	assert(targetReg.Count == static_cast<u32>(wide) + 1);

	const instruction_operand secondOperand = decodedInstruction.Operands[1];
	assert(secondOperand.Type == Operand_Immediate || secondOperand.Type == Operand_Register);

	const s16 sourceVal = (secondOperand.Type == Operand_Immediate) ?
							secondOperand.Immediate.Value & 0xFFFF :
							registers[secondOperand.Register.Index].GetValue();

	bool overflow = false;
	bool auxCarry = false;

	u32 result = 0;
	switch (decodedInstruction.Op) {
		case Op_add: {
			result = ToU32(targetVal) + ToU32(sourceVal);
			overflow = CheckOverflow(targetVal, sourceVal, static_cast<s16>(result));
			auxCarry = ((targetVal & 0x0F) + (sourceVal & 0x0F)) & 0x10;
			registers[targetReg.Index].SetValue(static_cast<u16>(result & 0x0000FFFF));
			break;
		}
		case Op_sub: {
			result = ToU32(targetVal) - ToU32(sourceVal);
			overflow = CheckOverflow(targetVal, -sourceVal, static_cast<s16>(result));
			auxCarry = ((targetVal & 0x0F) - (sourceVal & 0x0F)) & 0x10;
			registers[targetReg.Index].SetValue(static_cast<u16>(result & 0x0000FFFF));
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

	setBitFlag(flags, ZF, (static_cast<u16>(result) == 0));
	setBitFlag(flags, SF, (result & 0x8000));
	setBitFlag(flags, OF, overflow);
	setBitFlag(flags, AF, auxCarry);
	setBitFlag(flags, CF, (result & 0x00010000));
	setBitFlag(flags, PF, CheckParity(result));
}

void ExecuteInstruction(const instruction& decodedInstruction)
{
	switch (decodedInstruction.Op) {
		case Op_mov:
			ExecuteMov(decodedInstruction);
			break;
		case Op_cmp:
		case Op_add:
		case Op_sub:
			ExecuteArithmetic(decodedInstruction);
			break;
		default:
			printf("Unimplemented operation type\n");
			break;
	}
}

ExecutionContext GetCurrentState(const instruction decodedInstruction)
{
	register_access reg = decodedInstruction.Operands[0].Register;
	return { reg, registers[reg.Index].GetValue(), flags };
}

std::string GetActiveFlagNames(const u8 bitFlags)
{
	std::string result;
	for (size_t i = 0; i < ArrayCount(flagNames); ++i) {
		if (bitFlags & (1 << i)) {
			result.append(flagNames[i]);
		}
	}

	return result;
}

void PrintExecutionState(const instruction decodedInstruction, ExecutionContext context, FILE* Dest)
{
	assert(decodedInstruction.Operands[0].Type == Operand_Register);

	fprintf(Dest, " ;");

	const OperationPrintDetails printDetails = GetOpPrintDetails(decodedInstruction.Op);

	if (printDetails.writesResult) {
		const register_access reg = decodedInstruction.Operands[0].Register;
		fprintf(Dest, " %s:0x%x->", Sim86_RegisterNameFromOperand(&context.reg), static_cast<u16>(context.regValue));
		fprintf(Dest, "0x%x", static_cast<u16>(registers[reg.Index].GetValue()));
	}

	if (printDetails.flagModifier && (context.flags != flags)) {
		fprintf(Dest, " flags:%s->%s", GetActiveFlagNames(context.flags).c_str(), GetActiveFlagNames(flags).c_str());
	}

	fprintf(Dest, "\n");
}

void PrintFinalState(FILE* Dest)
{
	fprintf(Dest, "Final Registers:\n");

	for (size_t i = 1; i < ArrayCount(registers); ++i) {
		if (!registers[i].IsModified()) {
			continue;
		}

		register_access reg{ i, 0 , 2 };
		fprintf(Dest, "      %s:0x%x (%d)\n", Sim86_RegisterNameFromOperand(&reg),
				static_cast<u16>(registers[i].GetValue()), static_cast<u16>(registers[i].GetValue()));
	}

	fprintf(Dest, "flags: %s\n", GetActiveFlagNames(flags).c_str());
}


void Execute8086(std::vector<u8>& buffer)
{
	u8* instructions = &buffer[0];

	u32 offset = 0;
	while (offset < buffer.size()) {
		instruction decoded;
		Sim86_Decode8086Instruction(buffer.size() - offset, instructions + offset, &decoded);

		if (!decoded.Op) {
			printf("Unrecognized instruction.\n");
			break;
		}

		if (decoded.Operands[0].Type != Operand_Register) {
			printf("Not implemented.\n");
			break;
		}

		offset += decoded.Size;

		PrintInstruction(decoded, stdout);
		const ExecutionContext context = GetCurrentState(decoded);
		ExecuteInstruction(decoded);
		PrintExecutionState(decoded, context, stdout);
	}

	PrintFinalState(stdout);
}

void Disassemble8086(std::vector<u8>& buffer)
{
	printf("bits 16\n");

	u8* instructions = &buffer[0];

	u32 offset = 0;
	while (offset < buffer.size()) {
		instruction decoded;
		Sim86_Decode8086Instruction(buffer.size() - offset, instructions + offset, &decoded);
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
	}

	return 0;
}
