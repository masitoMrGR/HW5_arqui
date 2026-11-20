#pragma once

#include <cstdint>
#include <unordered_map>

// Memoria byte-addressable de 32 bits. Solo se guardan los bytes escritos;
// leer una direccion que nunca se escribio devuelve 0.
class Memory {
private:
    std::unordered_map<uint32_t, uint8_t> bytes;

public:
    void clear() { bytes.clear(); }

    uint8_t read8(uint32_t addr) const {
        auto it = bytes.find(addr);
        return it == bytes.end() ? 0 : it->second;
    }

    void write8(uint32_t addr, uint8_t value) { bytes[addr] = value; }

    // Little-endian: el byte menos significativo va en la direccion mas baja
    uint16_t read16(uint32_t addr) const {
        return read8(addr) | (read8(addr + 1) << 8);
    }

    uint32_t read32(uint32_t addr) const {
        return read16(addr) | ((uint32_t)read16(addr + 2) << 16);
    }

    void write16(uint32_t addr, uint16_t value) {
        write8(addr, value & 0xFF);
        write8(addr + 1, value >> 8);
    }

    void write32(uint32_t addr, uint32_t value) {
        write16(addr, value & 0xFFFF);
        write16(addr + 2, value >> 16);
    }
};
