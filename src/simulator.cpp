#include "simulator.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <vector>

#include "decode.hpp"

void Simulator::reset() {
    pc = 0;
    for (int i = 0; i < 32; i++)
        regs[i] = 0;
    mem.clear();
    halted = false;
    halt_reason = "";
    exit_code = 0;
    count = 0;
}

static uint32_t le32(const std::vector<uint8_t>& d, uint32_t i) {
    return d[i] | (d[i + 1] << 8) | (d[i + 2] << 16) | ((uint32_t)d[i + 3] << 24);
}

static uint32_t le16(const std::vector<uint8_t>& d, uint32_t i) {
    return d[i] | (d[i + 1] << 8);
}

std::string Simulator::load(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file)
        throw std::runtime_error("No se pudo abrir \"" + filename + "\"");
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    reset();

    bool elf = data.size() >= 52 && data[0] == 0x7F && data[1] == 'E' && data[2] == 'L' && data[3] == 'F';

    // Binario crudo: se copia tal cual desde la direccion 0
    if (!elf) {
        for (uint32_t i = 0; i < data.size(); i++)
            mem.write8(i, data[i]);
        return "\"" + filename + "\" cargado a memoria (binario crudo, " + std::to_string(data.size()) + " bytes).";
    }

    // ELF: se copian los segmentos PT_LOAD y el PC empieza en el entry point
    if (data[4] != 1 || le16(data, 18) != 243)
        throw std::runtime_error("El ELF no es de RISC-V de 32 bits");
    uint32_t phoff = le32(data, 28);
    uint32_t phsize = le16(data, 42);
    uint32_t phnum = le16(data, 44);
    for (uint32_t i = 0; i < phnum; i++) {
        uint32_t ph = phoff + i * phsize;
        if (le32(data, ph) != 1)
            continue;
        uint32_t offset = le32(data, ph + 4);
        uint32_t vaddr = le32(data, ph + 8);
        uint32_t filesz = le32(data, ph + 16);
        for (uint32_t j = 0; j < filesz; j++)
            mem.write8(vaddr + j, data[offset + j]);
    }
    pc = le32(data, 24);
    return "\"" + filename + "\" cargado a memoria (ELF, entry point " + hex(pc) + ").";
}

[[noreturn]] static void invalid(uint32_t ins, uint32_t pc) {
    throw std::runtime_error("Instruccion invalida " + hex(ins) + " en pc = " + hex(pc));
}

void Simulator::step() {
    if (halted)
        throw std::runtime_error("El programa ya termino. Usa \"reset\" para empezar de nuevo.");

    // Fetch
    uint32_t ins = mem.read32(pc);

    // Decode
    uint32_t rd = get_rd(ins);
    uint32_t f3 = get_funct3(ins);
    uint32_t f7 = get_funct7(ins);
    uint32_t a = regs[get_rs1(ins)];
    uint32_t b = regs[get_rs2(ins)];
    uint32_t next_pc = pc + 4;

    // Execute
    switch (get_opcode(ins)) {
        case OP_LUI:
            set_reg(rd, imm_u(ins));
            break;

        case OP_AUIPC:
            set_reg(rd, pc + imm_u(ins));
            break;

        case OP_JAL:
            next_pc = pc + imm_j(ins);
            set_reg(rd, pc + 4);
            break;

        case OP_JALR:
            next_pc = (a + imm_i(ins)) & ~1u;  // antes de escribir rd, por si rd == rs1
            set_reg(rd, pc + 4);
            break;

        case OP_BRANCH: {
            bool taken;
            if (f3 == 0) taken = a == b;                             // beq
            else if (f3 == 1) taken = a != b;                        // bne
            else if (f3 == 4) taken = (int32_t)a < (int32_t)b;       // blt
            else if (f3 == 5) taken = (int32_t)a >= (int32_t)b;      // bge
            else if (f3 == 6) taken = a < b;                         // bltu
            else if (f3 == 7) taken = a >= b;                        // bgeu
            else invalid(ins, pc);
            if (taken)
                next_pc = pc + imm_b(ins);
            break;
        }

        case OP_LOAD: {
            uint32_t addr = a + imm_i(ins);
            if (f3 == 0) set_reg(rd, (int8_t)mem.read8(addr));        // lb
            else if (f3 == 1) set_reg(rd, (int16_t)mem.read16(addr)); // lh
            else if (f3 == 2) set_reg(rd, mem.read32(addr));          // lw
            else if (f3 == 4) set_reg(rd, mem.read8(addr));           // lbu
            else if (f3 == 5) set_reg(rd, mem.read16(addr));          // lhu
            else invalid(ins, pc);
            break;
        }

        case OP_STORE: {
            uint32_t addr = a + imm_s(ins);
            if (f3 == 0) mem.write8(addr, b);        // sb
            else if (f3 == 1) mem.write16(addr, b);  // sh
            else if (f3 == 2) mem.write32(addr, b);  // sw
            else invalid(ins, pc);
            break;
        }

        case OP_IMM: {
            int32_t imm = imm_i(ins);
            uint32_t shamt = get_rs2(ins);
            if (f3 == 0) set_reg(rd, a + imm);                                    // addi
            else if (f3 == 2) set_reg(rd, (int32_t)a < imm);                      // slti
            else if (f3 == 3) set_reg(rd, a < (uint32_t)imm);                     // sltiu
            else if (f3 == 4) set_reg(rd, a ^ imm);                               // xori
            else if (f3 == 6) set_reg(rd, a | imm);                               // ori
            else if (f3 == 7) set_reg(rd, a & imm);                               // andi
            else if (f3 == 1 && f7 == 0x00) set_reg(rd, a << shamt);              // slli
            else if (f3 == 5 && f7 == 0x00) set_reg(rd, a >> shamt);              // srli
            else if (f3 == 5 && f7 == 0x20) set_reg(rd, (int32_t)a >> shamt);     // srai
            else invalid(ins, pc);
            break;
        }

        case OP_REG: {
            uint32_t shamt = b & 0x1F;
            if (f7 == 0x00 && f3 == 0) set_reg(rd, a + b);                        // add
            else if (f7 == 0x20 && f3 == 0) set_reg(rd, a - b);                   // sub
            else if (f7 == 0x00 && f3 == 1) set_reg(rd, a << shamt);              // sll
            else if (f7 == 0x00 && f3 == 2) set_reg(rd, (int32_t)a < (int32_t)b); // slt
            else if (f7 == 0x00 && f3 == 3) set_reg(rd, a < b);                   // sltu
            else if (f7 == 0x00 && f3 == 4) set_reg(rd, a ^ b);                   // xor
            else if (f7 == 0x00 && f3 == 5) set_reg(rd, a >> shamt);              // srl
            else if (f7 == 0x20 && f3 == 5) set_reg(rd, (int32_t)a >> shamt);     // sra
            else if (f7 == 0x00 && f3 == 6) set_reg(rd, a | b);                   // or
            else if (f7 == 0x00 && f3 == 7) set_reg(rd, a & b);                   // and
            else invalid(ins, pc);
            break;
        }

        case OP_SYSTEM:
            if (ins != 0x00000073)
                invalid(ins, pc);
            ecall();
            break;

        default:
            invalid(ins, pc);
    }

    if (next_pc % 4 != 0)
        throw std::runtime_error("Salto a una direccion no alineada: " + hex(next_pc));

    count++;

    // Un salto a si mismo ("j ." o "done: beq x2, x2, done") marca el fin del programa
    if (next_pc == pc) {
        halted = true;
        halt_reason = "bucle infinito en " + hex(pc);
    }

    pc = next_pc;
}

// Environment calls como en CPUlator: el servicio va en x17 y el argumento en x10
void Simulator::ecall() {
    uint32_t x10 = regs[10];
    uint32_t x17 = regs[17];

    if (x17 == 1) {  // imprimir entero
        std::cout << (int32_t)x10 << std::flush;
    } else if (x17 == 4) {  // imprimir string terminado en 0
        for (uint32_t p = x10; mem.read8(p) != 0; p++)
            std::cout << (char)mem.read8(p);
        std::cout << std::flush;
    } else if (x17 == 5) {  // leer entero
        int32_t n = 0;
        std::cin >> n;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        set_reg(10, n);
    } else if (x17 == 10) {  // exit
        halted = true;
        halt_reason = "ecall exit";
    } else if (x17 == 11) {  // imprimir caracter
        std::cout << (char)x10 << std::flush;
    } else if (x17 == 17) {  // exit con codigo en x10
        halted = true;
        exit_code = (int32_t)x10;
        halt_reason = "ecall exit con codigo " + std::to_string(exit_code);
    } else if (x17 == 34) {  // imprimir en hexadecimal
        std::cout << hex(x10) << std::flush;
    } else {
        throw std::runtime_error("ecall no soportada (x17 = " + std::to_string(x17) + ")");
    }
}
