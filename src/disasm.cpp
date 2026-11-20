#include <string>

#include "decode.hpp"

static std::string x(uint32_t r) {
    return "x" + std::to_string(r);
}

std::string disassemble(uint32_t ins, uint32_t pc) {
    uint32_t f3 = get_funct3(ins);
    uint32_t f7 = get_funct7(ins);
    std::string rd = x(get_rd(ins));
    std::string rs1 = x(get_rs1(ins));
    std::string rs2 = x(get_rs2(ins));
    std::string imm = std::to_string(imm_i(ins));
    std::string shamt = std::to_string(get_rs2(ins));
    std::string name;

    switch (get_opcode(ins)) {
        case OP_LUI:
            return "lui " + rd + ", " + std::to_string((uint32_t)imm_u(ins) >> 12);
        case OP_AUIPC:
            return "auipc " + rd + ", " + std::to_string((uint32_t)imm_u(ins) >> 12);
        case OP_JAL:
            return "jal " + rd + ", " + hex(pc + imm_j(ins));
        case OP_JALR:
            return "jalr " + rd + ", " + rs1 + ", " + imm;

        case OP_BRANCH:
            if (f3 == 0) name = "beq";
            else if (f3 == 1) name = "bne";
            else if (f3 == 4) name = "blt";
            else if (f3 == 5) name = "bge";
            else if (f3 == 6) name = "bltu";
            else if (f3 == 7) name = "bgeu";
            else break;
            return name + " " + rs1 + ", " + rs2 + ", " + hex(pc + imm_b(ins));

        case OP_LOAD:
            if (f3 == 0) name = "lb";
            else if (f3 == 1) name = "lh";
            else if (f3 == 2) name = "lw";
            else if (f3 == 4) name = "lbu";
            else if (f3 == 5) name = "lhu";
            else break;
            return name + " " + rd + ", " + imm + "(" + rs1 + ")";

        case OP_STORE:
            if (f3 == 0) name = "sb";
            else if (f3 == 1) name = "sh";
            else if (f3 == 2) name = "sw";
            else break;
            return name + " " + rs2 + ", " + std::to_string(imm_s(ins)) + "(" + rs1 + ")";

        case OP_IMM:
            if (f3 == 0) name = "addi";
            else if (f3 == 2) name = "slti";
            else if (f3 == 3) name = "sltiu";
            else if (f3 == 4) name = "xori";
            else if (f3 == 6) name = "ori";
            else if (f3 == 7) name = "andi";
            else if (f3 == 1 && f7 == 0x00) return "slli " + rd + ", " + rs1 + ", " + shamt;
            else if (f3 == 5 && f7 == 0x00) return "srli " + rd + ", " + rs1 + ", " + shamt;
            else if (f3 == 5 && f7 == 0x20) return "srai " + rd + ", " + rs1 + ", " + shamt;
            else break;
            return name + " " + rd + ", " + rs1 + ", " + imm;

        case OP_REG:
            if (f7 == 0x00 && f3 == 0) name = "add";
            else if (f7 == 0x20 && f3 == 0) name = "sub";
            else if (f7 == 0x00 && f3 == 1) name = "sll";
            else if (f7 == 0x00 && f3 == 2) name = "slt";
            else if (f7 == 0x00 && f3 == 3) name = "sltu";
            else if (f7 == 0x00 && f3 == 4) name = "xor";
            else if (f7 == 0x00 && f3 == 5) name = "srl";
            else if (f7 == 0x20 && f3 == 5) name = "sra";
            else if (f7 == 0x00 && f3 == 6) name = "or";
            else if (f7 == 0x00 && f3 == 7) name = "and";
            else break;
            return name + " " + rd + ", " + rs1 + ", " + rs2;

        case OP_SYSTEM:
            if (ins == 0x00000073) return "ecall";
            break;
    }
    return "<invalida>";
}
