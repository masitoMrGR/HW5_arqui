#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

enum Opcode : uint32_t {
    OP_LOAD = 0x03,
    OP_IMM = 0x13,
    OP_AUIPC = 0x17,
    OP_STORE = 0x23,
    OP_REG = 0x33,
    OP_LUI = 0x37,
    OP_BRANCH = 0x63,
    OP_JALR = 0x67,
    OP_JAL = 0x6F,
    OP_SYSTEM = 0x73,
};

inline uint32_t get_opcode(uint32_t ins) { return ins & 0x7F; }         // bits 6:0
inline uint32_t get_rd(uint32_t ins) { return (ins >> 7) & 0x1F; }      // bits 11:7
inline uint32_t get_funct3(uint32_t ins) { return (ins >> 12) & 0x7; }  // bits 14:12
inline uint32_t get_rs1(uint32_t ins) { return (ins >> 15) & 0x1F; }    // bits 19:15
inline uint32_t get_rs2(uint32_t ins) { return (ins >> 20) & 0x1F; }    // bits 24:20
inline uint32_t get_funct7(uint32_t ins) { return ins >> 25; }          // bits 31:25

// Castear a int32_t y desplazar a la derecha copia el bit 31 (extension de signo)

inline int32_t imm_i(uint32_t ins) {  // imm[11:0] = ins[31:20]
    return (int32_t)ins >> 20;
}

inline int32_t imm_s(uint32_t ins) {  // imm[11:5] = ins[31:25], imm[4:0] = ins[11:7]
    return ((int32_t)(ins & 0xFE000000) >> 20) | ((ins >> 7) & 0x1F);
}

inline int32_t imm_b(uint32_t ins) {
    return ((int32_t)(ins & 0x80000000) >> 19)  // ins[31]    -> imm[12]
           | ((ins & 0x80) << 4)                // ins[7]     -> imm[11]
           | ((ins >> 20) & 0x7E0)              // ins[30:25] -> imm[10:5]
           | ((ins >> 7) & 0x1E);               // ins[11:8]  -> imm[4:1]
}

inline int32_t imm_u(uint32_t ins) {  // imm[31:12] = ins[31:12]
    return ins & 0xFFFFF000;
}

inline int32_t imm_j(uint32_t ins) {
    return ((int32_t)(ins & 0x80000000) >> 11)  // ins[31]    -> imm[20]
           | (ins & 0xFF000)                    // ins[19:12] -> imm[19:12]
           | ((ins >> 9) & 0x800)               // ins[20]    -> imm[11]
           | ((ins >> 20) & 0x7FE);             // ins[30:21] -> imm[10:1]
}

inline std::string hex(uint32_t value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%08X", value);
    return buf;
}

std::string disassemble(uint32_t ins, uint32_t pc);
