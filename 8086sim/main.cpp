
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
const unsigned char opCodeAddSubReg = 0b00;
const unsigned char opCodeAddSubAcc = 0b100000;

const std::string oneByteRegs[] = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};
const std::string twoByteRegs[] = { "ax", "cx", "dx", "bx", "sp", "bp", "si", "di" };

const std::string effectiveAdressModes[] = { "bx + si", "bx + di", "bp + si", "bp + di", "si", "di", "bp", "bx" };

const std::string arithmeticOpCodes[] = { "add", "sub", "", "cmp" };

enum class MoveMode : char
{
	memoryModeNoDisp = 0,
	memoryModeDisp8 = 1,
	memoryModeDisp16 = 2,
	registerMode = 3
};

std::string readDataFromBuffer(std::stringstream& buffer, const bool isWord, const bool sign = false)
{
	if (!isWord) {
		int8_t data = buffer.get();
		return std::to_string(data);
	}

	const uint8_t lowData = buffer.get();
	const uint8_t highData = sign ?
							(lowData & 0x80) ? 0xff : 0x00 :
							buffer.get();
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
			address += effectiveAdressModes[rm];
			const int8_t dispLow = buffer.get();
			if (dispLow != 0) {
				const int8_t dispLowPositive = abs(dispLow);
				const std::string displacement = ((dispLow < 0) ? " - " : " + ") + std::to_string(dispLowPositive);
				address += displacement;
			}
			break;
		}
		case MoveMode::memoryModeDisp16: {
			address += effectiveAdressModes[rm];
			const uint8_t dispLow = buffer.get();
			const uint8_t dispHigh = buffer.get();
			const uint16_t dispValueUnsigned = (dispHigh << 8) + dispLow;
			const int16_t dispValueSigned = reinterpret_cast<const int16_t&>(dispValueUnsigned);
			if (dispValueSigned != 0) {
				const int16_t dispValuePositive = abs(dispValueSigned);
				const std::string displacement = ((dispValueSigned < 0) ? " - " : " + ") + std::to_string(dispValuePositive);
				address += displacement;
			}
			break;
		}
		default:
			assert(false);
			break;
	}

	return address;
}

std::string decodeRmReg(const char opCode, std::stringstream& buffer)
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
		return dest + ", " + src;
	}

	const std::string& regName = isWord ? twoByteRegs[regIdx] : oneByteRegs[regIdx];
	const std::string address = "[" + readEffectiveAddressFromBuffer(buffer, isWord, mod, rm) + "]";

	const std::string& dest = isToRegister ? regName : address;
	const std::string& src = isToRegister ? address : regName;
	return dest + ", " + src;
}

std::string decodeImmToRm(const unsigned char opCode, std::stringstream& buffer, const bool useSign = false)
{
	const bool isWord = opCode & 0x01;
	const bool sign = useSign ? (opCode & 0x02) : false;

	unsigned char operands = buffer.get();
	const unsigned char modChar = operands >> 6;
	const MoveMode mod = static_cast<MoveMode>(modChar);
	const unsigned char rm = operands & 0b00000111;

	if (mod == MoveMode::registerMode) {
		const std::string dest = isWord ? twoByteRegs[rm] : oneByteRegs[rm];
		const std::string src = readDataFromBuffer(buffer, isWord, sign);
		return dest + ", " + src;
	}

	const std::string dest = "[" + readEffectiveAddressFromBuffer(buffer, isWord, mod, rm) + "]";
	const std::string src = readDataFromBuffer(buffer, isWord, sign);
	return (isWord ? "word " : "byte ") + dest + ", " + src;
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
	const std::string acc = isWord ? "ax" : "al";
	const std::string address = readDataFromBuffer(buffer, isWord);
	return "mov " + toMemory ? ("[" + address + "], " + acc) : (acc + ", [" + address + "]");
}

std::string decodeArithRmReg(const unsigned char opCode, std::stringstream& buffer)
{
	const unsigned char idx = (opCode & 0b00011000) >> 3;
	return arithmeticOpCodes[idx] + " " + decodeRmReg(opCode, buffer);
}

std::string decodeArithImmToRm(const unsigned char opCode, std::stringstream& buffer)
{
	static const bool USE_SIGN = true;
	const unsigned char operands = buffer.peek();
	const unsigned char idx = (operands & 0b00011000) >> 3;
	return arithmeticOpCodes[idx] + " " + decodeImmToRm(opCode, buffer, USE_SIGN);
}

std::string decodeArithImmToAcc(const unsigned char opCode, std::stringstream& buffer)
{
	const bool isWord = opCode & 0x01;
	const std::string acc = isWord ? "ax" : "al";
	const std::string data = readDataFromBuffer(buffer, isWord);

	const unsigned char idx = (opCode & 0b00011000) >> 3;
	return arithmeticOpCodes[idx] + " " + acc + ", " + data;
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

	outputFile << "bits 16" << '\n' << std::endl;

	for (unsigned char opcodeByte = buffer.get(); !buffer.eof(); opcodeByte = buffer.get()) {
		std::string decodedInstruction;

		if ((opcodeByte >> 2) == opCodeMoveRmToReg) {
			decodedInstruction = "mov " + decodeRmReg(opcodeByte, buffer);
		}
		else if ((opcodeByte >> 1) == opCodeMoveImmToRm) {
			decodedInstruction = "mov " + decodeImmToRm(opcodeByte, buffer);
		}
		else if ((opcodeByte >> 4) == opCodeMoveImmToReg) {
			decodedInstruction = decodeMoveImmToReg(opcodeByte, buffer);
		}
		else if ((opcodeByte >> 2) == opCodeMoveMemAcc) {
			decodedInstruction = decodeMoveMemAcc(opcodeByte, buffer);
		}
		else if ((opcodeByte >> 6) == opCodeAddSubReg) {
			decodedInstruction = (opcodeByte & 0x04) ?
								 decodeArithImmToAcc(opcodeByte, buffer) :
								 decodeArithRmReg(opcodeByte, buffer);
		}
		else if ((opcodeByte >> 2) == opCodeAddSubAcc) {
			decodedInstruction = decodeArithImmToRm(opcodeByte, buffer);
		}

		outputFile << decodedInstruction << std::endl;
	}
}

int main()
{
	disassemble("test_files/listing_0040_challenge_movs");
}







