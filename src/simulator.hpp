#pragma once

#include <cstdint>
#include <string>

#include "memory.hpp"

class Simulator {
public:
    uint32_t pc = 0;
    uint32_t regs[32] = {};
    Memory mem;

    bool halted = false;
    std::string halt_reason;
    int exit_code = 0;
    uint64_t count = 0;  // instrucciones ejecutadas

    void reset();
    std::string load(const std::string& filename);  // lanza std::runtime_error si falla
    void step();                                     // ejecuta una instruccion

private:
    void set_reg(uint32_t r, uint32_t value) {
        if (r != 0)  // x0 siempre vale 0
            regs[r] = value;
    }

    void ecall();
};
