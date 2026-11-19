#include <cstdio>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "decode.hpp"
#include "simulator.hpp"

// Limite de seguridad para "run" (por si el programa nunca termina)
constexpr uint64_t RUN_LIMIT = 100000000;

static std::string hex8(uint32_t v) {
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%08X", v);
    return buf;
}

// Acepta "0x1000", "4096", "0b101"...
static bool parse_num(const std::string& s, uint32_t& out) {
    try {
        size_t used;
        if (s.size() > 2 && s[0] == '0' && (s[1] == 'b' || s[1] == 'B'))
            out = std::stoul(s.substr(2), &used, 2), used += 2;
        else
            out = std::stoul(s, &used, 0);
        return used == s.size();
    } catch (...) {
        return false;
    }
}

// Acepta "x5", "t0", "zero", "fp"
static int parse_reg(const std::string& s) {
    if (s.size() >= 2 && s[0] == 'x') {
        uint32_t n;
        if (parse_num(s.substr(1), n) && n < 32)
            return n;
    }
    if (s == "fp")
        return 8;
    for (int i = 0; i < 32; i++)
        if (s == REG_NAMES[i])
            return i;
    return -1;
}

static void print_reg(const Simulator& sim, int r) {
    char buf[96];
    uint32_t v = sim.get_reg(r);
    snprintf(buf, sizeof(buf), "x%-2d (%-4s) = 0x%08X  (%d)", r, REG_NAMES[r], v, (int32_t)v);
    std::cout << buf << "\n";
}

static void print_all_regs(const Simulator& sim) {
    std::cout << "pc = " << hex8(sim.get_pc()) << "\n";
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 4; j++) {
            int r = i + 8 * j;
            char buf[48];
            snprintf(buf, sizeof(buf), "x%-2d %-4s 0x%08X   ", r, REG_NAMES[r], sim.get_reg(r));
            std::cout << buf;
        }
        std::cout << "\n";
    }
}

static void print_instr(const Simulator& sim, uint32_t addr) {
    uint32_t ins = sim.memory().read32(addr);
    char buf[48];
    snprintf(buf, sizeof(buf), "%s %s:  %08X   ", addr == sim.get_pc() ? "=>" : "  ", hex8(addr).c_str(), ins);
    std::cout << buf << disassemble(ins, addr) << "\n";
}

static void print_help() {
    std::cout << "Comandos:\n"
                 "  step [n]           Ejecuta 1 (o n) instrucciones\n"
                 "  run                Ejecuta hasta terminar, un breakpoint o un ebreak\n"
                 "  pc                 Muestra el PC\n"
                 "  regs [r1 r2 ...]   Muestra todos los registros o solo los pedidos (x5, a0, sp...)\n"
                 "  mem <ini> [fin]    Muestra los bytes de memoria de ini a fin (inclusive)\n"
                 "  memw <dir> [n]     Muestra n palabras de 32 bits desde dir\n"
                 "  dis [dir] [n]      Desensambla n instrucciones desde dir (por defecto, alrededor del PC)\n"
                 "  break <dir>        Pone un breakpoint\n"
                 "  delete <dir>       Quita un breakpoint\n"
                 "  breaks             Lista los breakpoints\n"
                 "  reset              Recarga el programa y reinicia el estado\n"
                 "  load <archivo>     Carga otro programa (.bin crudo o .elf)\n"
                 "  help               Muestra esta ayuda\n"
                 "  exit               Sale del simulador\n";
}

// Ejecuta una instruccion y muestra que hizo (instruccion + registros que cambiaron)
static bool do_step(Simulator& sim, bool verbose) {
    uint32_t old_pc = sim.get_pc();
    uint32_t before[32];
    for (int i = 0; i < 32; i++)
        before[i] = sim.get_reg(i);

    try {
        sim.step();
    } catch (const SimError& e) {
        std::cout << "Error: " << e.what() << "\n";
        return false;
    }

    if (verbose) {
        uint32_t ins = sim.memory().read32(old_pc);
        char buf[48];
        snprintf(buf, sizeof(buf), "%s:  %08X   ", hex8(old_pc).c_str(), ins);
        std::cout << buf << disassemble(ins, old_pc);
        for (int i = 1; i < 32; i++)
            if (sim.get_reg(i) != before[i])
                std::cout << "    ; " << REG_NAMES[i] << " = " << hex8(sim.get_reg(i));
        std::cout << "\n";
    }
    if (sim.is_halted()) {
        std::cout << "Programa terminado: " << sim.get_halt_reason() << ". (" << sim.get_instr_count()
                  << " instrucciones ejecutadas)\n";
        return false;
    }
    if (sim.take_ebreak()) {
        std::cout << "ebreak en " << hex8(old_pc) << "\n";
        return false;
    }
    return true;
}

int main(int argc, char** argv) {
    // Uso: rvsim programa.bin [--echo]
    // --echo vuelve a imprimir cada comando leido (util cuando los comandos vienen de un pipe)
    std::string program;
    bool echo = false;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--echo")
            echo = true;
        else
            program = arg;
    }
    if (program.empty()) {
        std::cerr << "Uso: " << argv[0] << " <programa.bin | programa.elf> [--echo]\n";
        return 1;
    }

    Simulator sim;
    std::string msg;
    if (!sim.load_file(program, msg)) {
        std::cerr << msg << "\n";
        return 1;
    }
    std::cout << msg << "\n";
    std::cout << "Escribe \"help\" para ver los comandos.\n";

    std::set<uint32_t> breakpoints;
    std::string line;

    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line))
            break;
        if (!line.empty() && line.back() == '\r')  // Windows
            line.pop_back();
        if (echo)
            std::cout << line << "\n";

        std::istringstream ss(line);
        std::vector<std::string> args;
        std::string tok;
        while (ss >> tok)
            args.push_back(tok);
        if (args.empty())
            continue;
        std::string cmd = args[0];

        if (cmd == "exit" || cmd == "quit" || cmd == "q") {
            break;
        } else if (cmd == "help" || cmd == "h") {
            print_help();
        } else if (cmd == "step" || cmd == "s") {
            uint32_t n = 1;
            if (args.size() > 1 && !parse_num(args[1], n)) {
                std::cout << "Numero invalido.\n";
                continue;
            }
            for (uint32_t i = 0; i < n; i++)
                if (!do_step(sim, n <= 50))
                    break;
            if (n > 50)
                std::cout << "pc = " << hex8(sim.get_pc()) << "\n";
        } else if (cmd == "run" || cmd == "r") {
            uint64_t count = 0;
            while (true) {
                if (!do_step(sim, false))
                    break;
                if (breakpoints.count(sim.get_pc())) {
                    std::cout << "Breakpoint en " << hex8(sim.get_pc()) << "\n";
                    print_instr(sim, sim.get_pc());
                    break;
                }
                if (++count >= RUN_LIMIT) {
                    std::cout << "Se alcanzo el limite de " << RUN_LIMIT << " instrucciones.\n";
                    break;
                }
            }
        } else if (cmd == "pc") {
            std::cout << "pc = " << hex8(sim.get_pc()) << "\n";
        } else if (cmd == "regs" || cmd == "reg") {
            if (args.size() == 1) {
                print_all_regs(sim);
                continue;
            }
            for (size_t i = 1; i < args.size(); i++) {
                if (args[i] == "pc") {
                    std::cout << "pc = " << hex8(sim.get_pc()) << "\n";
                    continue;
                }
                int r = parse_reg(args[i]);
                if (r < 0)
                    std::cout << "Registro desconocido: " << args[i] << "\n";
                else
                    print_reg(sim, r);
            }
        } else if (cmd == "mem") {
            uint32_t ini, fin;
            if (args.size() < 2 || !parse_num(args[1], ini)) {
                std::cout << "Uso: mem <ini> [fin]\n";
                continue;
            }
            fin = ini + 15;
            if (args.size() > 2 && !parse_num(args[2], fin)) {
                std::cout << "Uso: mem <ini> [fin]\n";
                continue;
            }
            if (fin < ini || fin - ini > 4095) {
                std::cout << "Rango invalido (maximo 4096 bytes).\n";
                continue;
            }
            std::cout << "Memoria (" << hex8(ini) << "-" << hex8(fin) << "):";
            for (uint32_t addr = ini; ; addr++) {
                if ((addr - ini) % 16 == 0)
                    std::cout << "\n  " << hex8(addr) << ":";
                char buf[8];
                snprintf(buf, sizeof(buf), " %02X", sim.memory().read8(addr));
                std::cout << buf;
                if (addr == fin)
                    break;
            }
            std::cout << "\n";
        } else if (cmd == "memw") {
            uint32_t addr, n = 1;
            if (args.size() < 2 || !parse_num(args[1], addr) || (args.size() > 2 && !parse_num(args[2], n))) {
                std::cout << "Uso: memw <dir> [n]\n";
                continue;
            }
            for (uint32_t i = 0; i < n && i < 1024; i++) {
                uint32_t v = sim.memory().read32(addr + 4 * i);
                char buf[64];
                snprintf(buf, sizeof(buf), "  %s: 0x%08X  (%d)", hex8(addr + 4 * i).c_str(), v, (int32_t)v);
                std::cout << buf << "\n";
            }
        } else if (cmd == "dis") {
            uint32_t addr = sim.get_pc() >= 8 ? sim.get_pc() - 8 : 0, n = 8;
            if (args.size() > 1 && !parse_num(args[1], addr)) {
                std::cout << "Uso: dis [dir] [n]\n";
                continue;
            }
            if (args.size() > 2 && !parse_num(args[2], n)) {
                std::cout << "Uso: dis [dir] [n]\n";
                continue;
            }
            addr &= ~3u;
            for (uint32_t i = 0; i < n && i < 1024; i++)
                print_instr(sim, addr + 4 * i);
        } else if (cmd == "break" || cmd == "b") {
            uint32_t addr;
            if (args.size() < 2 || !parse_num(args[1], addr)) {
                std::cout << "Uso: break <dir>\n";
                continue;
            }
            breakpoints.insert(addr);
            std::cout << "Breakpoint en " << hex8(addr) << "\n";
        } else if (cmd == "delete") {
            uint32_t addr;
            if (args.size() < 2 || !parse_num(args[1], addr)) {
                std::cout << "Uso: delete <dir>\n";
                continue;
            }
            if (breakpoints.erase(addr))
                std::cout << "Breakpoint eliminado.\n";
            else
                std::cout << "No habia un breakpoint ahi.\n";
        } else if (cmd == "breaks") {
            if (breakpoints.empty())
                std::cout << "No hay breakpoints.\n";
            for (uint32_t addr : breakpoints)
                std::cout << "  " << hex8(addr) << "\n";
        } else if (cmd == "reset") {
            sim.load_file(program, msg);
            std::cout << msg << "\n";
        } else if (cmd == "load") {
            if (args.size() < 2) {
                std::cout << "Uso: load <archivo>\n";
                continue;
            }
            if (sim.load_file(args[1], msg))
                program = args[1];
            std::cout << msg << "\n";
        } else {
            std::cout << "Comando desconocido: " << cmd << ". Escribe \"help\".\n";
        }
    }

    std::cout << "See you next time...\n";
    std::cout << "Program exited with code " << sim.get_exit_code() << ".\n";
    return sim.get_exit_code();
}
