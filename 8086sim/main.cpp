
#include <fstream>
#include <sstream>
#include <iostream>
#include <bitset>
#include <assert.h>

// Leftmost bits
const unsigned char opCodeMoveRmToReg = 0b100010;
const unsigned char opCodeMoveImmToRm = 0b1100011;
const unsigned char opCodeMoveImmToReg = 0b1011;
const unsigned char opCodeMoveMemAcc = 0b101000;

const std::string oneByteRegs[] = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};
const std::string twoByteRegs[] = { "ax", "cx", "dx", "bx", "sp", "bp", "si", "di" };

const std::string effectiveAdressModes[] = { "bx + si", "bx + di", "bp + si", "bp + di", "si", "di", "bp", "bx" };

enum class MoveMode : char
{
	memoryModeNoDisp = 0,
	memoryModeDisp8 = 1,
	memoryModeDisp16 = 2,
	registerMode = 3
};

std::string readDataFromBuffer(std::stringstream& buffer, const bool isWord)
{
	if (!isWord) {
		int8_t data = buffer.get();
		return std::to_string(data);
	}

	const uint8_t lowData = buffer.get();
	const uint8_t highData = buffer.get();
	const uint16_t dataUnsigned = (highData << 8) + lowData;
	const int16_t dataSigned = reinterpret_cast<const int16_t&>(dataUnsigned);
	return std::to_string(dataSigned);
}

std::string readEffectiveAddressFromBuffer(std::stringstream& buffer, const bool isWord, const MoveMode mode, const unsigned char rm)
{
	std::string address;

	switch (mode) {
		case MoveMode::memoryModeNoDisp: {
			if (rm == 6) {
				const std::string directAddress = readDataFromBuffer(buffer, true);
				address += directAddress;
			}
			else {
				address += effectiveAdressModes[rm];
			}
			break;
		}
		case MoveMode::memoryModeDisp8: {
			const int8_t dispLow = buffer.get();
			if (dispLow != 0) {
				const int8_t dispLowPositive = abs(dispLow);
				const std::string displacement = ((dispLow < 0) ? " - " : " + ") + std::to_string(dispLowPositive);
				address += effectiveAdressModes[rm] + displacement;
			}
			break;
		}
		case MoveMode::memoryModeDisp16: {
			const uint8_t dispLow = buffer.get();
			const uint8_t dispHigh = buffer.get();
			const uint16_t dispValueUnsigned = (dispHigh << 8) + dispLow;
			const int16_t dispValueSigned = reinterpret_cast<const int16_t&>(dispValueUnsigned);
			if (dispValueSigned != 0) {
				const int16_t dispValuePositive = abs(dispValueSigned);
				const std::string displacement = ((dispValueSigned < 0) ? " - " : " + ") + std::to_string(dispValuePositive);
				address += effectiveAdressModes[rm] + displacement;
			}
			break;
		}
		default:
			assert(false);
			break;
	}

	return address;
}

std::string decodeMoveRmToReg(const char opCode, std::stringstream& buffer)
{
	const bool isToRegister = opCode & 0x02;
	const bool isWord = opCode & 0x01;

	unsigned char operands = buffer.get();

	const unsigned char modChar = operands >> 6;
	const MoveMode mod = static_cast<MoveMode>(modChar);
	const unsigned char regIdx = (operands & 0b00111000) >> 3;
	const unsigned char rm = operands & 0b00000111;

	if (mod == MoveMode::registerMode) {
		const std::string& reg1 = isWord ? twoByteRegs[regIdx] : oneByteRegs[regIdx];
		const std::string& reg2 = isWord ? twoByteRegs[rm] : oneByteRegs[rm];
		const std::string& dest = isToRegister ? reg1 : reg2;
		const std::string& src = isToRegister ? reg2 : reg1;

		const std::string decodedInstruction = "mov " + dest + ", " + src;
		return decodedInstruction;
	}

	const std::string& regName = isWord ? twoByteRegs[regIdx] : oneByteRegs[regIdx];
	const std::string address = "[" + readEffectiveAddressFromBuffer(buffer, isWord, mod, rm) + "]";

	const std::string& dest = isToRegister ? regName : address;
	const std::string& src = isToRegister ? address : regName;
	return "mov " + dest + ", " + src;
}

std::string decodeMoveImmToRm(const char opCode, std::stringstream& buffer)
{
	const bool isWord = opCode & 0x01;

	unsigned char operands = buffer.get();
	const unsigned char modChar = operands >> 6;
	const MoveMode mod = static_cast<MoveMode>(modChar);
	const unsigned char rm = operands & 0b00000111;

	if (mod == MoveMode::registerMode) {
		const std::string dest = isWord ? twoByteRegs[rm] : oneByteRegs[rm];
		const std::string src = readDataFromBuffer(buffer, isWord);
		return "mov " + dest + ", " + src;
	}

	const std::string dest = "[" + readEffectiveAddressFromBuffer(buffer, isWord, mod, rm) + "]";
	const std::string src = (isWord ? "word " : " byte ") + readDataFromBuffer(buffer, isWord);
	return "mov " + dest + ", " + src;
}

std::string decodeMoveImmToReg(const char opCode, std::stringstream& buffer)
{
	const bool isWord = opCode & 0b00001000;
	const unsigned char regIdx = opCode & 0b00000111;
	const std::string dest = isWord ? twoByteRegs[regIdx] : oneByteRegs[regIdx];
	const std::string src = readDataFromBuffer(buffer, isWord);

	return "mov " + dest + ", " + src;
}

std::string decodeMoveMemAcc(const char opCode, std::stringstream& buffer)
{
	const bool isWord = opCode & 0x01;
	const bool toMemory = opCode & 0x02;
	const std::string address = readDataFromBuffer(buffer, isWord);
	return toMemory ? "mov [" + address + "], ax" : "mov ax, [" + address + "]";
}

{
}

void disassemble(const std::string& fileName)
{
	std::ofstream outputFile("test_files/output.asm");
	if (!outputFile.is_open()) {
		std::cout << "Failed to open output file!" << std::endl;
		return;
	}

	std::fstream fstream{ fileName, fstream.binary | fstream.in};
	if (!fstream.is_open()) {
		std::cout << "Failed to open!" << std::endl;
		return;
	}

	std::stringstream buffer;
	buffer << fstream.rdbuf();
	fstream.close();

	std::cout << "bits 16" << std::endl;

	for (unsigned char opcodeByte = buffer.get(); !buffer.eof(); opcodeByte = buffer.get()) {
		std::string decodedInstruction;

		if ((opcodeByte >> 2) == opCodeMoveRmToReg) {
			decodedInstruction = decodeMoveRmToReg(opcodeByte, buffer);
		}
		else if ((opcodeByte >> 1) == opCodeMoveImmToRm) {
			decodedInstruction = decodeMoveImmToRm(opcodeByte, buffer);
		}
		else if ((opcodeByte >> 4) == opCodeMoveImmToReg) {
			decodedInstruction = decodeMoveImmToReg(opcodeByte, buffer);
		}
		else if ((opcodeByte >> 2) == opCodeMoveMemAcc) {
			decodedInstruction = decodeMoveMemAcc(opcodeByte, buffer);
		}
		}
		}

		outputFile << decodedInstruction << std::endl;
	}
}

int main()
{
	disassemble("test_files/listing_0040_challenge_movs");
}







