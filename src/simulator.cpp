#include "simulator.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>

#include "decode.hpp"

void Simulator::reset() {
    pc = 0;
    for (int i = 0; i < 32; i++)
        regs[i] = 0;
    mem.clear();
    halted = false;
    exit_code = 0;
    halt_reason = "";
    ebreak_hit = false;
    instr_count = 0;
}

// ---------------------------------------------------------------------------
// Carga del programa
// ---------------------------------------------------------------------------

static uint32_t le32(const std::vector<uint8_t>& d, size_t off) {
    return d[off] | (d[off + 1] << 8) | (d[off + 2] << 16) | ((uint32_t)d[off + 3] << 24);
}

static uint16_t le16(const std::vector<uint8_t>& d, size_t off) {
    return d[off] | (d[off + 1] << 8);
}

bool Simulator::load_file(const std::string& filename, std::string& msg) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        msg = "No se pudo abrir \"" + filename + "\".";
        return false;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    reset();

    bool is_elf = data.size() >= 52 && data[0] == 0x7F && data[1] == 'E' && data[2] == 'L' && data[3] == 'F';

    if (!is_elf) {
        // Binario crudo: los bytes del archivo se copian tal cual desde 0x00000000
        for (size_t i = 0; i < data.size(); i++)
            mem.write8(i, data[i]);
        pc = 0;
        msg = "\"" + filename + "\" cargado a memoria (binario crudo, " + std::to_string(data.size()) +
              " bytes en 0x00000000).";
        return true;
    }

    // ELF de 32 bits, little-endian, maquina RISC-V (243)
    if (data[4] != 1 || data[5] != 1 || le16(data, 18) != 243) {
        msg = "El ELF no es RISC-V de 32 bits little-endian.";
        return false;
    }
    uint32_t entry = le32(data, 24);
    uint32_t phoff = le32(data, 28);
    uint16_t phentsize = le16(data, 42);
    uint16_t phnum = le16(data, 44);

    int segments = 0;
    for (int i = 0; i < phnum; i++) {
        size_t ph = phoff + (size_t)i * phentsize;
        if (ph + 32 > data.size())
            break;
        uint32_t type = le32(data, ph);
        uint32_t offset = le32(data, ph + 4);
        uint32_t vaddr = le32(data, ph + 8);
        uint32_t filesz = le32(data, ph + 16);
        uint32_t memsz = le32(data, ph + 20);
        if (type != 1)  // solo segmentos PT_LOAD
            continue;
        for (uint32_t j = 0; j < memsz; j++) {
            uint8_t b = 0;  // lo que pasa de filesz (p. ej. .bss) va en 0
            if (j < filesz && offset + j < data.size())
                b = data[offset + j];
            mem.write8(vaddr + j, b);
        }
        segments++;
    }
    pc = entry;
    char buf[32];
    snprintf(buf, sizeof(buf), "0x%08X", entry);
    msg = "\"" + filename + "\" cargado a memoria (ELF, " + std::to_string(segments) +
          " segmento(s), entry point " + buf + ").";
    return true;
}

// ---------------------------------------------------------------------------
// Ejecucion
// ---------------------------------------------------------------------------

static std::string hex8(uint32_t v) {
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%08X", v);
    return buf;
}

void Simulator::step() {
    if (halted)
        throw SimError("El programa ya termino (" + halt_reason + "). Usa \"reset\" para reiniciar.");
    if (pc % 4 != 0)
        throw SimError("PC desalineado: " + hex8(pc));

    // 1) FETCH: leer la instruccion apuntada por el PC
    uint32_t ins = mem.read32(pc);

    // 2) DECODE: separar los campos
    uint32_t opcode = get_opcode(ins);
    uint32_t rd = get_rd(ins);
    uint32_t f3 = get_funct3(ins);
    uint32_t f7 = get_funct7(ins);
    uint32_t a = regs[get_rs1(ins)];  // valor de rs1
    uint32_t b = regs[get_rs2(ins)];  // valor de rs2

    // Por defecto la siguiente instruccion es PC + 4; saltos y branches lo cambian
    uint32_t next_pc = pc + 4;

    auto check_target = [&](uint32_t target) {
        if (target % 4 != 0)
            throw SimError("Salto a direccion desalineada " + hex8(target) + " desde PC = " + hex8(pc));
    };

    auto invalid = [&]() {
        char buf[64];
        snprintf(buf, sizeof(buf), "Instruccion invalida 0x%08X en PC = 0x%08X", ins, pc);
        throw SimError(buf);
    };

    // 3) EXECUTE
    switch (opcode) {
        case OP_LUI:  // rd = imm << 12
            set_reg(rd, imm_u(ins));
            break;

        case OP_AUIPC:  // rd = PC + (imm << 12)
            set_reg(rd, pc + imm_u(ins));
            break;

        case OP_JAL:  // rd = PC + 4; PC = PC + offset
            next_pc = pc + imm_j(ins);
            check_target(next_pc);
            set_reg(rd, pc + 4);
            break;

        case OP_JALR:  // rd = PC + 4; PC = (rs1 + imm) & ~1
            if (f3 != 0)
                invalid();
            next_pc = (a + imm_i(ins)) & ~1u;  // se calcula ANTES de escribir rd (rd puede ser rs1)
            check_target(next_pc);
            set_reg(rd, pc + 4);
            break;

        case OP_BRANCH: {
            bool taken;
            switch (f3) {
                case 0: taken = a == b; break;                    // beq
                case 1: taken = a != b; break;                    // bne
                case 4: taken = (int32_t)a < (int32_t)b; break;   // blt  (con signo)
                case 5: taken = (int32_t)a >= (int32_t)b; break;  // bge  (con signo)
                case 6: taken = a < b; break;                     // bltu (sin signo)
                case 7: taken = a >= b; break;                    // bgeu (sin signo)
                default: invalid(); return;
            }
            if (taken) {
                next_pc = pc + imm_b(ins);
                check_target(next_pc);
            }
            break;
        }

        case OP_LOAD: {
            uint32_t addr = a + imm_i(ins);
            uint32_t value;
            switch (f3) {
                case 0: value = (int32_t)(int8_t)mem.read8(addr); break;    // lb  (extiende signo)
                case 1: value = (int32_t)(int16_t)mem.read16(addr); break;  // lh  (extiende signo)
                case 2: value = mem.read32(addr); break;                    // lw
                case 4: value = mem.read8(addr); break;                     // lbu (rellena con 0)
                case 5: value = mem.read16(addr); break;                    // lhu (rellena con 0)
                default: invalid(); return;
            }
            set_reg(rd, value);
            break;
        }

        case OP_STORE: {
            uint32_t addr = a + imm_s(ins);
            switch (f3) {
                case 0: mem.write8(addr, b & 0xFF); break;      // sb
                case 1: mem.write16(addr, b & 0xFFFF); break;   // sh
                case 2: mem.write32(addr, b); break;            // sw
                default: invalid(); return;
            }
            break;
        }

        case OP_IMM: {
            int32_t imm = imm_i(ins);
            uint32_t shamt = get_rs2(ins);  // en los shifts, los bits 24:20 son el shamt
            uint32_t value;
            switch (f3) {
                case 0: value = a + imm; break;                               // addi
                case 2: value = (int32_t)a < imm ? 1 : 0; break;              // slti
                case 3: value = a < (uint32_t)imm ? 1 : 0; break;             // sltiu
                case 4: value = a ^ imm; break;                               // xori
                case 6: value = a | imm; break;                               // ori
                case 7: value = a & imm; break;                               // andi
                case 1:                                                       // slli
                    if (f7 != 0x00) invalid();
                    value = a << shamt;
                    break;
                case 5:
                    if (f7 == 0x00) value = a >> shamt;                       // srli
                    else if (f7 == 0x20) value = (int32_t)a >> shamt;         // srai
                    else { invalid(); return; }
                    break;
                default: invalid(); return;
            }
            set_reg(rd, value);
            break;
        }

        case OP_REG: {
            uint32_t sh = b & 0x1F;  // solo cuentan los 5 bits bajos de rs2
            uint32_t value;
            if (f7 == 0x00) {
                switch (f3) {
                    case 0: value = a + b; break;                                // add
                    case 1: value = a << sh; break;                              // sll
                    case 2: value = (int32_t)a < (int32_t)b ? 1 : 0; break;      // slt
                    case 3: value = a < b ? 1 : 0; break;                        // sltu
                    case 4: value = a ^ b; break;                                // xor
                    case 5: value = a >> sh; break;                              // srl
                    case 6: value = a | b; break;                                // or
                    case 7: value = a & b; break;                                // and
                    default: invalid(); return;
                }
            } else if (f7 == 0x20 && f3 == 0) {
                value = a - b;                                                   // sub
            } else if (f7 == 0x20 && f3 == 5) {
                value = (int32_t)a >> sh;                                        // sra
            } else {
                invalid();
                return;
            }
            set_reg(rd, value);
            break;
        }

        case OP_MISC:  // fence: en un simulador de un solo hilo no hace nada
            break;

        case OP_SYSTEM:
            if (ins == 0x00000073) {
                do_ecall();
            } else if (ins == 0x00100073) {
                ebreak_hit = true;
            } else {
                invalid();
            }
            break;

        default:
            invalid();
    }

    instr_count++;

    // Un salto a si mismo (p. ej. "j ." o "done: beq x2, x2, done") es la forma
    // tipica de terminar un programa bare-metal. Lo tomamos como fin del programa.
    if (next_pc == pc && !halted) {
        halted = true;
        halt_reason = "bucle infinito en " + hex8(pc) + ", fin del programa";
    }

    // 4) Actualizar el PC
    pc = next_pc;
}

// ---------------------------------------------------------------------------
// Environment calls estilo SPIM/CPUlator (numero de servicio en a7)
// ---------------------------------------------------------------------------

void Simulator::do_ecall() {
    uint32_t a0 = regs[10], a1 = regs[11], a7 = regs[17];
    switch (a7) {
        case 1:  // print integer
            std::cout << (int32_t)a0 << std::flush;
            break;
        case 4:  // print string (a0 = puntero a string terminada en \0)
            for (uint32_t p = a0; mem.read8(p) != 0; p++)
                std::cout << (char)mem.read8(p);
            std::cout << std::flush;
            break;
        case 5: {  // read integer -> a0
            int32_t x = 0;
            std::cin >> x;
            set_reg(10, x);
            break;
        }
        case 8: {  // read string: a0 = buffer, a1 = tamano maximo
            std::string line;
            std::getline(std::cin >> std::ws, line);
            uint32_t n = 0;
            for (; a1 > 0 && n + 1 < a1 && n < line.size(); n++)
                mem.write8(a0 + n, line[n]);
            if (a1 > 0)
                mem.write8(a0 + n, 0);
            break;
        }
        case 10:  // exit
            halted = true;
            exit_code = 0;
            halt_reason = "ecall exit";
            break;
        case 11:  // print character
            std::cout << (char)(a0 & 0xFF) << std::flush;
            break;
        case 12: {  // read character -> a0
            char c = 0;
            std::cin.get(c);
            set_reg(10, (uint8_t)c);
            break;
        }
        case 17:  // exit2 (codigo en a0)
            halted = true;
            exit_code = (int32_t)a0;
            halt_reason = "ecall exit2 con codigo " + std::to_string((int32_t)a0);
            break;
        case 34: {  // print integer en hex
            char buf[16];
            snprintf(buf, sizeof(buf), "0x%08x", a0);
            std::cout << buf << std::flush;
            break;
        }
        case 35: {  // print integer en binario
            for (int i = 31; i >= 0; i--)
                std::cout << ((a0 >> i) & 1);
            std::cout << std::flush;
            break;
        }
        case 36:  // print integer sin signo
            std::cout << a0 << std::flush;
            break;
        default:
            throw SimError("ecall no soportada (a7 = " + std::to_string(a7) + ")");
    }
}
