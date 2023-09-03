
#include <fstream>
#include <iostream>
#include <vector>
#include <assert.h>

#include "shared/sim86_shared.h"
#include "text_utils.h"

u16 registerValues[13] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

void ExecuteInstruction(const instruction& decodedInstruction)
{
    if (decodedInstruction.Op != Op_mov) {
        return;
    }

    const bool W = decodedInstruction.Flags & Inst_Wide;

    const register_access targetReg = decodedInstruction.Operands[0].Register;
    assert(targetReg.Count == W + 1);

    instruction_operand secondOperand = decodedInstruction.Operands[1];
    switch (secondOperand.Type) {
        case Operand_Immediate: {

            u16& target = registerValues[targetReg.Index];
            const s16 immVal = secondOperand.Immediate.Value & 0xFFFF;
            if (W) {
                target = immVal;
            }
            else {
                const u16 source = immVal & 0xFF;
                const bool isTargetHigh = targetReg.Offset & 1;
                target = isTargetHigh ? (target & 0x00FF) | (source << 8) : (target & 0xFF00) | source;
            }

            break;
        }
        case Operand_Register: {
            const register_access sourceReg = decodedInstruction.Operands[1].Register;
            const u16 sourceRegVal = registerValues[sourceReg.Index];
            u16& target = registerValues[targetReg.Index];

            if (W) {
                target = sourceRegVal;
            }
            else {
                assert(sourceReg.Count == 1);

                const bool isTargetHigh = targetReg.Offset & 1;
                const bool isSourceHigh = sourceReg.Offset & 1;
                const u16 source = isSourceHigh ? ((sourceRegVal & 0xFF00) >> 8) : (sourceRegVal & 0x00FF);
                target = isTargetHigh ? (target & 0x00FF) | (source << 8) : (target & 0xFF00) | source;
            }

            break;
        }
        default:
            printf("Unimplemented operand type\n");
    }
}

void PrintRegisterStateChange(instruction decodedInstruction, FILE* Dest)
{
    if (decodedInstruction.Operands[0].Type != Operand_Register) {
        return;
    }

    fprintf(Dest, " ; ");

    register_access reg = decodedInstruction.Operands[0].Register;
    fprintf(Dest, "%s:0x%x->", Sim86_RegisterNameFromOperand(&reg), registerValues[reg.Index]);
    ExecuteInstruction(decodedInstruction);
    fprintf(Dest, "0x%x", registerValues[reg.Index]);
}

void PrintFinalRegisterState(FILE* Dest)
{
    fprintf(Dest, "Final Registers:\n");

    for (size_t i = 1; i < ArrayCount(registerValues); ++i) {
        register_access reg{ i, 0 , 2};
        fprintf(Dest, "      %s:0x%x (%d)\n", Sim86_RegisterNameFromOperand(&reg), registerValues[i], registerValues[i]);
    }
}


void Execute8086(std::vector<u8>& buffer)
{
    u8* instructions = &buffer[0];

    u32 Offset = 0;
    while (Offset < buffer.size()) {
        instruction Decoded;
        Sim86_Decode8086Instruction(buffer.size() - Offset, instructions + Offset, &Decoded);

        if (!Decoded.Op) {
            printf("Unrecognized instruction\n");
            break;
        }

        Offset += Decoded.Size;
        PrintInstruction(Decoded, stdout);
        PrintRegisterStateChange(Decoded, stdout);
        printf("\n");
    }

    PrintFinalRegisterState(stdout);
}

void Disassemble8086(std::vector<u8>& buffer)
{
    printf("bits 16\n");

    u8* instructions = &buffer[0];

    u32 Offset = 0;
    while (Offset < buffer.size()) {
        instruction Decoded;
        Sim86_Decode8086Instruction(buffer.size() - Offset, instructions + Offset, &Decoded);
        if (Decoded.Op) {
            Offset += Decoded.Size;
            PrintInstruction(Decoded, stdout);
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
