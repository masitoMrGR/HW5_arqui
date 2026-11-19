#include <cstdio>
#include <string>

#include "decode.hpp"

const char* REG_NAMES[32] = {
    "zero", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "s0", "s1", "a0",
    "a1",   "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",
    "s6",   "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6",
};

static std::string hex(uint32_t value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%x", value);
    return buf;
}

std::string disassemble(uint32_t ins, uint32_t pc) {
    uint32_t opcode = get_opcode(ins);
    uint32_t f3 = get_funct3(ins);
    uint32_t f7 = get_funct7(ins);
    std::string rd = REG_NAMES[get_rd(ins)];
    std::string rs1 = REG_NAMES[get_rs1(ins)];
    std::string rs2 = REG_NAMES[get_rs2(ins)];

    switch (opcode) {
        case OP_LUI:
            return "lui " + rd + ", " + hex((uint32_t)imm_u(ins) >> 12);
        case OP_AUIPC:
            return "auipc " + rd + ", " + hex((uint32_t)imm_u(ins) >> 12);
        case OP_JAL:
            return "jal " + rd + ", " + hex(pc + imm_j(ins));
        case OP_JALR:
            return "jalr " + rd + ", " + std::to_string(imm_i(ins)) + "(" + rs1 + ")";
        case OP_BRANCH: {
            const char* names[8] = {"beq", "bne", "?", "?", "blt", "bge", "bltu", "bgeu"};
            if (names[f3][0] == '?')
                break;
            return std::string(names[f3]) + " " + rs1 + ", " + rs2 + ", " + hex(pc + imm_b(ins));
        }
        case OP_LOAD: {
            const char* names[8] = {"lb", "lh", "lw", "?", "lbu", "lhu", "?", "?"};
            if (names[f3][0] == '?')
                break;
            return std::string(names[f3]) + " " + rd + ", " + std::to_string(imm_i(ins)) + "(" + rs1 + ")";
        }
        case OP_STORE: {
            const char* names[8] = {"sb", "sh", "sw", "?", "?", "?", "?", "?"};
            if (names[f3][0] == '?')
                break;
            return std::string(names[f3]) + " " + rs2 + ", " + std::to_string(imm_s(ins)) + "(" + rs1 + ")";
        }
        case OP_IMM: {
            uint32_t shamt = get_rs2(ins);
            switch (f3) {
                case 0: return "addi " + rd + ", " + rs1 + ", " + std::to_string(imm_i(ins));
                case 2: return "slti " + rd + ", " + rs1 + ", " + std::to_string(imm_i(ins));
                case 3: return "sltiu " + rd + ", " + rs1 + ", " + std::to_string(imm_i(ins));
                case 4: return "xori " + rd + ", " + rs1 + ", " + std::to_string(imm_i(ins));
                case 6: return "ori " + rd + ", " + rs1 + ", " + std::to_string(imm_i(ins));
                case 7: return "andi " + rd + ", " + rs1 + ", " + std::to_string(imm_i(ins));
                case 1:
                    if (f7 == 0x00) return "slli " + rd + ", " + rs1 + ", " + std::to_string(shamt);
                    break;
                case 5:
                    if (f7 == 0x00) return "srli " + rd + ", " + rs1 + ", " + std::to_string(shamt);
                    if (f7 == 0x20) return "srai " + rd + ", " + rs1 + ", " + std::to_string(shamt);
                    break;
            }
            break;
        }
        case OP_REG: {
            std::string args = " " + rd + ", " + rs1 + ", " + rs2;
            if (f7 == 0x00) {
                const char* names[8] = {"add", "sll", "slt", "sltu", "xor", "srl", "or", "and"};
                return names[f3] + args;
            }
            if (f7 == 0x20 && f3 == 0) return "sub" + args;
            if (f7 == 0x20 && f3 == 5) return "sra" + args;
            break;
        }
        case OP_MISC:
            return "fence";
        case OP_SYSTEM:
            if (ins == 0x00000073) return "ecall";
            if (ins == 0x00100073) return "ebreak";
            break;
    }
    return "<invalida>";
}
