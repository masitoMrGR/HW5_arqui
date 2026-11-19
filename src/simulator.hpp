#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "memory.hpp"

// Error de ejecucion (instruccion invalida, salto desalineado, ecall desconocida...)
struct SimError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Estado arquitectural de una CPU RV32I + la logica para ejecutar instrucciones.
class Simulator {
private:
    uint32_t pc = 0;
    uint32_t regs[32] = {};
    Memory mem;

    bool halted = false;
    int exit_code = 0;
    std::string halt_reason;
    bool ebreak_hit = false;
    uint64_t instr_count = 0;

    void set_reg(uint32_t r, uint32_t value) {
        if (r != 0)  // x0 siempre vale 0: las escrituras se ignoran
            regs[r] = value;
    }

    void do_ecall();

public:
    void reset();
    bool load_file(const std::string& filename, std::string& msg);

    // Ejecuta UNA instruccion: fetch -> decode -> execute -> actualizar PC.
    // Lanza SimError si la instruccion es invalida (el estado no cambia).
    void step();

    uint32_t get_pc() const { return pc; }
    uint32_t get_reg(uint32_t r) const { return regs[r]; }
    const Memory& memory() const { return mem; }
    bool is_halted() const { return halted; }
    int get_exit_code() const { return exit_code; }
    const std::string& get_halt_reason() const { return halt_reason; }
    uint64_t get_instr_count() const { return instr_count; }

    // true si la ultima instruccion fue un ebreak (sirve para pausar "run")
    bool take_ebreak() {
        bool b = ebreak_hit;
        ebreak_hit = false;
        return b;
    }
};
