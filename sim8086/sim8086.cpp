
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
	Register_none,

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

class RegisterInfo
{
	public:
		inline u16 GetValue() const
		{
			return value;
		}

		inline void SetValue(const u16 newValue)
		{
			value = newValue;
			modified = true;
		}

		inline bool IsModified()
		{
			return modified;
		}

	private:
		u16 value = 0;
		bool modified = false;
};

RegisterInfo registers[Register_count];

const u8 CF = 1;
const u8 PF = 1 << 1;
const u8 AF = 1 << 2;
const u8 ZF = 1 << 3;
const u8 SF = 1 << 4;
const u8 OF = 1 << 5;

constexpr u32 ToU32(const s16 value)
{
	return static_cast<u32>(static_cast<u16>(value));
}

/**
 * @brief Returned structure from execution calls that stores info about modified registers.
 */
struct ExecutionInfo
{
	RegisterType modifiedRegister = Register_none;
	bool flagModified = false;
};

void setBitFlag(u16& bitflags, const u8 flag, const bool set)
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

ExecutionInfo ExecuteMov(const instruction& decodedInstruction)
{
	assert(decodedInstruction.Op == Op_mov);

	const bool wide = decodedInstruction.Flags & Inst_Wide;

	const register_access targetReg = decodedInstruction.Operands[0].Register;
	assert(targetReg.Count == static_cast<u32>(wide) + 1);

	instruction_operand secondOperand = decodedInstruction.Operands[1];
	switch (secondOperand.Type) {
		case Operand_Immediate: {
			const u16 immVal = secondOperand.Immediate.Value & 0xFFFF;
			if (wide) {
				registers[targetReg.Index].SetValue(immVal);
			}
			else {
				const u16 targetVal = registers[targetReg.Index].GetValue();
				const u16 source = immVal & 0xFF;
				const bool isTargetHigh = targetReg.Offset & 1;
				const u16 result = isTargetHigh ? (targetVal & 0x00FF) | (source << 8) : (targetVal & 0xFF00) | source;
				registers[targetReg.Index].SetValue(result);
			}

			break;
		}
		case Operand_Register: {
			const register_access sourceReg = decodedInstruction.Operands[1].Register;
			const u16 sourceVal = registers[sourceReg.Index].GetValue();

			if (wide) {
				registers[targetReg.Index].SetValue(sourceVal);
			}
			else {
				assert(sourceReg.Count == 1);

				const bool isTargetHigh = targetReg.Offset & 1;
				const bool isSourceHigh = sourceReg.Offset & 1;
				const u16 targetVal = registers[targetReg.Index].GetValue();
				const u16 source = isSourceHigh ? ((sourceVal & 0xFF00) >> 8) : (sourceVal & 0x00FF);
				const u16 result = isTargetHigh ? (targetVal & 0x00FF) | (source << 8) : (targetVal & 0xFF00) | source;
				registers[targetReg.Index].SetValue(result);
			}

			break;
		}
		default:
			printf("Unimplemented operand type\n");
	}

	return { static_cast<RegisterType>(targetReg.Index), false };
}

ExecutionInfo ExecuteArithmetic(const instruction& decodedInstruction)
{
	assert((decodedInstruction.Op == Op_add) || (decodedInstruction.Op == Op_sub) || (decodedInstruction.Op == Op_cmp));

	ExecutionInfo executionInfo;

	const bool wide = decodedInstruction.Flags & Inst_Wide;

	const register_access targetReg = decodedInstruction.Operands[0].Register;
	const s16 targetVal = static_cast<s16>(registers[targetReg.Index].GetValue());
	assert(targetReg.Count == static_cast<u32>(wide) + 1);

	const instruction_operand secondOperand = decodedInstruction.Operands[1];
	assert(secondOperand.Type == Operand_Immediate || secondOperand.Type == Operand_Register);

	const s16 sourceVal = (secondOperand.Type == Operand_Immediate) ?
							secondOperand.Immediate.Value & 0xFFFF :
							static_cast<s16>(registers[secondOperand.Register.Index].GetValue());

	bool overflow = false;
	bool auxCarry = false;

	u32 result = 0;
	switch (decodedInstruction.Op) {
		case Op_add: {
			result = ToU32(targetVal) + ToU32(sourceVal);
			overflow = CheckOverflow(targetVal, sourceVal, static_cast<s16>(result));
			auxCarry = ((targetVal & 0x0F) + (sourceVal & 0x0F)) & 0x10;
			registers[targetReg.Index].SetValue(static_cast<u16>(result & 0x0000FFFF));
			executionInfo.modifiedRegister = static_cast<RegisterType>(targetReg.Index);
			break;
		}
		case Op_sub: {
			result = ToU32(targetVal) - ToU32(sourceVal);
			overflow = CheckOverflow(targetVal, -sourceVal, static_cast<s16>(result));
			auxCarry = ((targetVal & 0x0F) - (sourceVal & 0x0F)) & 0x10;
			registers[targetReg.Index].SetValue(static_cast<u16>(result & 0x0000FFFF));
			executionInfo.modifiedRegister = static_cast<RegisterType>(targetReg.Index);
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

	u16 flags = registers[Register_flags].GetValue();
	setBitFlag(flags, ZF, (static_cast<u16>(result) == 0));
	setBitFlag(flags, SF, (result & 0x8000));
	setBitFlag(flags, OF, overflow);
	setBitFlag(flags, AF, auxCarry);
	setBitFlag(flags, CF, (result & 0x00010000));
	setBitFlag(flags, PF, CheckParity(result));
	registers[Register_flags].SetValue(flags);
	executionInfo.flagModified = true;

	return executionInfo;
}

ExecutionInfo ExecuteJump(const instruction& decodedInstruction)
{
	ExecutionInfo executionInfo;

	bool jump = false;

	switch (decodedInstruction.Op) {
		case Op_je:
			jump = (registers[Register_flags].GetValue() & ZF);
			break;
		case Op_jne:
			jump = !(registers[Register_flags].GetValue() & ZF);
			break;
		case Op_jp:
			jump = (registers[Register_flags].GetValue() & PF);
			break;
		case Op_jb:
			jump = (registers[Register_flags].GetValue() & CF);
			break;
		case Op_loopnz: {
			u16 cx = registers[Register_c].GetValue() - 1;
			registers[Register_c].SetValue(cx);
			executionInfo.modifiedRegister = Register_c;

			jump = (cx != 0) && (!(registers[Register_flags].GetValue() & ZF));
			break;
		}
		default:
			break;
	}

	if (!jump) {
		return executionInfo;
	}

	const s16 jumpOffset = static_cast<s16>(decodedInstruction.Operands[0].Immediate.Value);
	const s16 ip = static_cast<s16>(registers[Register_ip].GetValue());
	registers[Register_ip].SetValue(static_cast<u16>(ip + jumpOffset));

	return executionInfo;
}


ExecutionInfo ExecuteInstruction(const instruction& decodedInstruction)
{
	switch (decodedInstruction.Op) {
		case Op_mov:
			return ExecuteMov(decodedInstruction);
			break;
		case Op_cmp:
		case Op_add:
		case Op_sub:
			return ExecuteArithmetic(decodedInstruction);
			break;
		case Op_je:
		case Op_jne:
		case Op_jp:
		case Op_jb:
		case Op_loopnz:
			return ExecuteJump(decodedInstruction);
			break;
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

void PrintExecutionState(const ExecutionInfo modifyInfo, const RegisterInfo (&backupRegisters)[Register_count], FILE* Dest)
{
	fprintf(Dest, " ;");

	if (modifyInfo.modifiedRegister != Register_none) {
		register_access reg = { modifyInfo.modifiedRegister, 0, 2 };
		fprintf(Dest, " %s:", Sim86_RegisterNameFromOperand(&reg));
		fprintf(Dest, "0x%x->0x%x", backupRegisters[reg.Index].GetValue(), registers[reg.Index].GetValue());
	}

	// Print ip register
	fprintf(Dest, " ip:0x%x->0x%x", backupRegisters[Register_ip].GetValue(), registers[Register_ip].GetValue());

	const u16 flags = registers[Register_flags].GetValue();
	const u16 backupFlags = backupRegisters[Register_flags].GetValue();
	if (modifyInfo.flagModified) {
		fprintf(Dest, " flags:%s->%s", GetActiveFlagNames(backupFlags).c_str(), GetActiveFlagNames(flags).c_str());
	}

	fprintf(Dest, "\n");
}

void PrintFinalState(FILE* Dest)
{
	fprintf(Dest, "Final Registers:\n");

	for (u16 i = 1; i < Register_flags; ++i) {
		if (!registers[i].IsModified()) {
			continue;
		}

		register_access reg{ i, 0 , 2 };
		fprintf(Dest, "      %s: 0x%.4x (%d)\n", Sim86_RegisterNameFromOperand(&reg), registers[i].GetValue(), registers[i].GetValue());
	}


	const u16 flags = registers[Register_flags].GetValue();
	fprintf(Dest, "   flags: %s\n", GetActiveFlagNames(flags).c_str());
}


void Execute8086(std::vector<u8>& buffer)
{
	u8* instructions = &buffer[0];
	const u32 bufferSize = static_cast<u32>(buffer.size());

	while (registers[Register_ip].GetValue() < bufferSize) {
		const u16 ip = registers[Register_ip].GetValue();
		instruction decoded;
		Sim86_Decode8086Instruction(bufferSize - ip, instructions + ip, &decoded);

		if (!decoded.Op) {
			printf("Unrecognized instruction.\n");
			break;
		}

		if (decoded.Operands[0].Type == Operand_Memory) {
			printf("Not implemented.\n");
			break;
		}

		RegisterInfo registersBeforeExecution[Register_count];
		std::memcpy(registersBeforeExecution, registers, Register_count * sizeof(RegisterInfo));

		registers[Register_ip].SetValue(ip + decoded.Size);

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
