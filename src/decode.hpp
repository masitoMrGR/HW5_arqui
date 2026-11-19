#pragma once

#include <cstdint>
#include <string>

// Opcodes (bits 6:0) del ISA base RV32I
enum Opcode : uint32_t {
    OP_LOAD = 0x03,    // lb lh lw lbu lhu          (tipo I)
    OP_MISC = 0x0F,    // fence                     (se trata como nop)
    OP_IMM = 0x13,     // addi slli slti ... andi   (tipo I)
    OP_AUIPC = 0x17,   // auipc                     (tipo U)
    OP_STORE = 0x23,   // sb sh sw                  (tipo S)
    OP_REG = 0x33,     // add sub sll ... and       (tipo R)
    OP_LUI = 0x37,     // lui                       (tipo U)
    OP_BRANCH = 0x63,  // beq bne blt bge bltu bgeu (tipo B)
    OP_JALR = 0x67,    // jalr                      (tipo I)
    OP_JAL = 0x6F,     // jal                       (tipo J)
    OP_SYSTEM = 0x73,  // ecall ebreak
};

// Campos que siempre estan en la misma posicion (eso es a proposito en RISC-V:
// rd, rs1 y rs2 nunca se mueven, asi el hardware los puede leer antes de saber
// que instruccion es).
inline uint32_t get_opcode(uint32_t ins) { return ins & 0x7F; }         // bits 6:0
inline uint32_t get_rd(uint32_t ins) { return (ins >> 7) & 0x1F; }      // bits 11:7
inline uint32_t get_funct3(uint32_t ins) { return (ins >> 12) & 0x7; }  // bits 14:12
inline uint32_t get_rs1(uint32_t ins) { return (ins >> 15) & 0x1F; }    // bits 19:15
inline uint32_t get_rs2(uint32_t ins) { return (ins >> 20) & 0x1F; }    // bits 24:20
inline uint32_t get_funct7(uint32_t ins) { return ins >> 25; }          // bits 31:25

// Inmediatos. Truco: castear a int32_t y hacer >> (shift aritmetico) copia el
// bit 31 hacia la izquierda, o sea, hace la extension de signo gratis.

// Tipo I: imm[11:0] = ins[31:20]
inline int32_t imm_i(uint32_t ins) {
    return (int32_t)ins >> 20;
}

// Tipo S: imm[11:5] = ins[31:25], imm[4:0] = ins[11:7]
inline int32_t imm_s(uint32_t ins) {
    return ((int32_t)(ins & 0xFE000000) >> 20) | ((ins >> 7) & 0x1F);
}

// Tipo B: imm[12] = ins[31], imm[11] = ins[7], imm[10:5] = ins[30:25],
//         imm[4:1] = ins[11:8], imm[0] = 0
inline int32_t imm_b(uint32_t ins) {
    return ((int32_t)(ins & 0x80000000) >> 19)  // ins[31]    -> imm[12] (y signo)
           | ((ins & 0x80) << 4)                // ins[7]     -> imm[11]
           | ((ins >> 20) & 0x7E0)              // ins[30:25] -> imm[10:5]
           | ((ins >> 7) & 0x1E);               // ins[11:8]  -> imm[4:1]
}

// Tipo U: imm[31:12] = ins[31:12], los 12 bits de abajo en 0
inline int32_t imm_u(uint32_t ins) {
    return (int32_t)(ins & 0xFFFFF000);
}

// Tipo J: imm[20] = ins[31], imm[19:12] = ins[19:12], imm[11] = ins[20],
//         imm[10:1] = ins[30:21], imm[0] = 0
inline int32_t imm_j(uint32_t ins) {
    return ((int32_t)(ins & 0x80000000) >> 11)  // ins[31]    -> imm[20] (y signo)
           | (ins & 0xFF000)                    // ins[19:12] -> imm[19:12]
           | ((ins >> 9) & 0x800)               // ins[20]    -> imm[11]
           | ((ins >> 20) & 0x7FE);             // ins[30:21] -> imm[10:1]
}

// Nombres ABI de los registros (x0..x31)
extern const char* REG_NAMES[32];

// Devuelve el texto de la instruccion, p. ej. "addi sp, sp, -16".
// pc se usa para mostrar el destino absoluto de saltos y branches.
std::string disassemble(uint32_t ins, uint32_t pc);
