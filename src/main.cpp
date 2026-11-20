#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "decode.hpp"
#include "simulator.hpp"

// Lee un numero en decimal (100) o hexadecimal (0x64)
static bool parse_num(const std::string& s, uint32_t& out) {
    try {
        size_t used;
        out = std::stoul(s, &used, 0);
        return used == s.size();
    } catch (...) {
        return false;
    }
}

// Lee un registro de la forma x0..x31
static int parse_reg(const std::string& s) {
    uint32_t r;
    if (s.size() >= 2 && s[0] == 'x' && parse_num(s.substr(1), r) && r < 32)
        return r;
    return -1;
}

static void print_reg(const Simulator& sim, int r) {
    std::cout << "x" << r << " = " << hex(sim.regs[r]) << "  (" << (int32_t)sim.regs[r] << ")\n";
}

static void print_all_regs(const Simulator& sim) {
    std::cout << "pc = " << hex(sim.pc) << "\n";
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 4; j++) {
            int r = i + 8 * j;
            std::string name = "x" + std::to_string(r);
            std::cout << name << std::string(5 - name.size(), ' ') << hex(sim.regs[r]) << "    ";
        }
        std::cout << "\n";
    }
}

static void print_instr(const Simulator& sim, uint32_t addr) {
    uint32_t ins = sim.mem.read32(addr);
    std::cout << (addr == sim.pc ? "=> " : "   ") << hex(addr) << ":  " << hex(ins) << "   "
              << disassemble(ins, addr) << "\n";
}

// Ejecuta una instruccion y muestra que hizo. Devuelve false si hay que detenerse.
static bool do_step(Simulator& sim, bool show) {
    uint32_t old_pc = sim.pc;
    uint32_t old_regs[32];
    for (int i = 0; i < 32; i++)
        old_regs[i] = sim.regs[i];

    try {
        sim.step();
    } catch (const std::runtime_error& e) {
        std::cout << "Error: " << e.what() << "\n";
        return false;
    }

    if (show) {
        uint32_t ins = sim.mem.read32(old_pc);
        std::cout << hex(old_pc) << ":  " << hex(ins) << "   " << disassemble(ins, old_pc);
        for (int i = 1; i < 32; i++)
            if (sim.regs[i] != old_regs[i])
                std::cout << "    ; x" << i << " = " << hex(sim.regs[i]);
        std::cout << "\n";
    }

    if (sim.halted) {
        std::cout << "Programa terminado: " << sim.halt_reason << " (" << sim.count << " instrucciones ejecutadas)\n";
        return false;
    }
    return true;
}

static void print_help() {
    std::cout << "  step [n]          ejecuta 1 (o n) instrucciones\n"
                 "  run               ejecuta hasta terminar o llegar a un breakpoint\n"
                 "  pc                muestra el pc\n"
                 "  regs [x5 x10 ...] muestra todos los registros o solo los indicados\n"
                 "  mem <ini> [fin]   muestra los bytes de memoria de ini a fin\n"
                 "  memw <dir> [n]    muestra n palabras de 32 bits desde dir\n"
                 "  dis [dir] [n]     desensambla n instrucciones desde dir\n"
                 "  break <dir>       pone un breakpoint\n"
                 "  delete <dir>      quita un breakpoint\n"
                 "  reset             recarga el programa\n"
                 "  exit              sale del simulador\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Uso: " << argv[0] << " <programa.bin | programa.elf>\n";
        return 1;
    }

    std::string program = argv[1];
    Simulator sim;
    try {
        std::cout << sim.load(program) << "\n";
    } catch (const std::runtime_error& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    std::cout << "Escribe \"help\" para ver los comandos.\n";

    std::set<uint32_t> breakpoints;
    std::string line;

    while (std::cout << "> " && std::getline(std::cin, line)) {
        std::istringstream ss(line);
        std::vector<std::string> args;
        std::string word;
        while (ss >> word)
            args.push_back(word);
        if (args.empty())
            continue;

        std::string cmd = args[0];
        uint32_t n1 = 0, n2 = 0;
        bool has1 = args.size() > 1 && parse_num(args[1], n1);
        bool has2 = args.size() > 2 && parse_num(args[2], n2);

        if (cmd == "exit") {
            break;
        } else if (cmd == "help") {
            print_help();
        } else if (cmd == "step") {
            uint32_t n = has1 ? n1 : 1;
            for (uint32_t i = 0; i < n; i++)
                if (!do_step(sim, true))
                    break;
        } else if (cmd == "run") {
            while (do_step(sim, false)) {
                if (breakpoints.count(sim.pc)) {
                    std::cout << "Breakpoint en " << hex(sim.pc) << "\n";
                    print_instr(sim, sim.pc);
                    break;
                }
            }
        } else if (cmd == "pc") {
            std::cout << "pc = " << hex(sim.pc) << "\n";
        } else if (cmd == "regs") {
            if (args.size() == 1)
                print_all_regs(sim);
            for (size_t i = 1; i < args.size(); i++) {
                int r = parse_reg(args[i]);
                if (r < 0)
                    std::cout << "Registro invalido: " << args[i] << " (usa x0 a x31)\n";
                else
                    print_reg(sim, r);
            }
        } else if (cmd == "mem") {
            uint32_t fin = has2 ? n2 : n1 + 15;
            if (!has1 || fin < n1) {
                std::cout << "Uso: mem <ini> [fin]\n";
                continue;
            }
            std::cout << "Memoria (" << hex(n1) << " - " << hex(fin) << "):";
            for (uint32_t i = 0; i <= fin - n1 && i < 1024; i++) {
                if (i % 16 == 0)
                    std::cout << "\n  " << hex(n1 + i) << ":";
                char buf[8];
                snprintf(buf, sizeof(buf), " %02X", sim.mem.read8(n1 + i));
                std::cout << buf;
            }
            std::cout << "\n";
        } else if (cmd == "memw") {
            if (!has1) {
                std::cout << "Uso: memw <dir> [n]\n";
                continue;
            }
            uint32_t n = has2 ? n2 : 1;
            for (uint32_t i = 0; i < n; i++) {
                uint32_t v = sim.mem.read32(n1 + 4 * i);
                std::cout << "  " << hex(n1 + 4 * i) << ": " << hex(v) << "  (" << (int32_t)v << ")\n";
            }
        } else if (cmd == "dis") {
            uint32_t addr = has1 ? n1 : sim.pc;
            uint32_t n = has2 ? n2 : 8;
            for (uint32_t i = 0; i < n; i++)
                print_instr(sim, addr + 4 * i);
        } else if (cmd == "break" && has1) {
            breakpoints.insert(n1);
            std::cout << "Breakpoint en " << hex(n1) << "\n";
        } else if (cmd == "delete" && has1) {
            breakpoints.erase(n1);
            std::cout << "Breakpoint eliminado\n";
        } else if (cmd == "reset") {
            std::cout << sim.load(program) << "\n";
        } else {
            std::cout << "Comando no valido. Escribe \"help\".\n";
        }
    }

    std::cout << "See you next time...\n";
    std::cout << "Program exited with code " << sim.exit_code << ".\n";
    return sim.exit_code;
}
