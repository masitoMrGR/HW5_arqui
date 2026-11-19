#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>

// Memoria plana byte-addressable de 32 bits (4 GiB de address space).
// No reservamos 4 GiB reales: la memoria se divide en paginas de 4 KiB que
// solo se crean cuando alguien escribe en ellas. Leer una pagina que no
// existe devuelve 0.
class Memory {
private:
    static constexpr uint32_t PAGE_SIZE = 4096;
    std::unordered_map<uint32_t, std::array<uint8_t, PAGE_SIZE>> pages;

public:
    void clear() { pages.clear(); }

    uint8_t read8(uint32_t addr) const {
        auto it = pages.find(addr / PAGE_SIZE);
        if (it == pages.end())
            return 0;
        return it->second[addr % PAGE_SIZE];
    }

    void write8(uint32_t addr, uint8_t value) {
        auto it = pages.find(addr / PAGE_SIZE);
        if (it == pages.end())
            it = pages.emplace(addr / PAGE_SIZE, std::array<uint8_t, PAGE_SIZE>{}).first;
        it->second[addr % PAGE_SIZE] = value;
    }

    // Little-endian: el byte MENOS significativo va en la direccion mas baja.
    uint16_t read16(uint32_t addr) const {
        return read8(addr) | (read8(addr + 1) << 8);
    }

    uint32_t read32(uint32_t addr) const {
        return (uint32_t)read8(addr) | ((uint32_t)read8(addr + 1) << 8) |
               ((uint32_t)read8(addr + 2) << 16) | ((uint32_t)read8(addr + 3) << 24);
    }

    void write16(uint32_t addr, uint16_t value) {
        write8(addr, value & 0xFF);
        write8(addr + 1, (value >> 8) & 0xFF);
    }

    void write32(uint32_t addr, uint32_t value) {
        write8(addr, value & 0xFF);
        write8(addr + 1, (value >> 8) & 0xFF);
        write8(addr + 2, (value >> 16) & 0xFF);
        write8(addr + 3, (value >> 24) & 0xFF);
    }
};
