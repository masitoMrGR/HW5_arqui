# rvsim: simulador de RISC-V (RV32I)

Simulador por terminal del ISA base RV32I, hecho en C++17 para la Tarea 5 de CS3051 Arquitectura de Computadores (2026-II).
Carga un programa ya compilado (binario crudo o ELF), lo ejecuta instrucción por instrucción y deja ver el PC, los registros y la memoria en cualquier momento.

## Compilar

```sh
make            # genera build/rvsim
```

Solo necesita `g++` (o `clang++`) con C++17. No usa librerías externas.
También trae `CMakeLists.txt`, así que se puede abrir directo en CLion (File → Open → carpeta `rvsim`) o compilar con `cmake -B build && cmake --build build`.

## Uso

```sh
./build/rvsim programa.bin     # binario crudo, se carga en 0x00000000
./build/rvsim programa.elf     # también acepta ELF de RISC-V 32 bits
```

```
$ ./build/rvsim tests/riscvtest.bin
"tests/riscvtest.bin" cargado a memoria (binario crudo, 84 bytes en 0x00000000).
> step
0x00000000:  00500113   addi sp, zero, 5    ; sp = 0x00000005
> run
Programa terminado: bucle infinito en 0x00000050, fin del programa. (19 instrucciones ejecutadas)
> mem 0x64 0x67
Memoria (0x00000064-0x00000067):
  0x00000064: 19 00 00 00
> exit
See you next time...
Program exited with code 0.
```

| Comando | Qué hace |
|---|---|
| `step [n]` | Ejecuta 1 (o n) instrucciones. Muestra la instrucción ejecutada y los registros que cambió |
| `run` | Ejecuta hasta que el programa termine, llegue a un breakpoint o a un `ebreak` |
| `pc` | Muestra el PC |
| `regs [r ...]` | Sin argumentos muestra los 32 registros; si no, solo los pedidos (`x5`, `a0`, `sp`, `pc`...) |
| `mem <ini> [fin]` | Bytes de memoria de `ini` a `fin` (inclusive), 16 por fila |
| `memw <dir> [n]` | `n` palabras de 32 bits desde `dir`, en hex y decimal |
| `dis [dir] [n]` | Desensambla `n` instrucciones (por defecto alrededor del PC; `=>` marca el PC) |
| `break <dir>` / `delete <dir>` / `breaks` | Poner, quitar y listar breakpoints |
| `reset` | Recarga el programa y reinicia todo el estado |
| `load <archivo>` | Carga otro programa |
| `help`, `exit` | Ayuda y salir |

Los números se pueden escribir en decimal (`100`), hex (`0x64`) o binario (`0b1100100`).
Los comandos también se pueden pasar por un pipe, útil para pruebas: `printf "run\nregs a0\n" | ./build/rvsim prog.bin --echo` (con `--echo` se ven los comandos en la salida).

## Features

Obligatorios:
- Las 37 instrucciones del ISA base RV32I (tabla del anexo A), más `fence` (como nop), `ecall` y `ebreak`.
- Memoria plana byte-addressable de 32 bits en little-endian.
- Carga de binarios crudos en `0x00000000`.
- Ejecución paso a paso e inspección de PC, registros y memoria.

Adicionales:
- `ecall` con los servicios de SPIM/CPUlator (número de servicio en `a7`): 1 print int, 4 print string, 5 read int, 8 read string, 10 exit, 11 print char, 12 read char, 17 exit2, 34 print hex, 35 print binario, 36 print unsigned.
- Carga directa de archivos `.elf` (segmentos `PT_LOAD` y entry point).
- Desensamblador (`dis`, y en cada `step`).
- Breakpoints y `ebreak`.
- Errores explícitos: instrucción inválida, PC o salto desalineado, `ecall` no soportada.

## Cómo está hecho

```
src/memory.hpp     Memoria de 32 bits por páginas de 4 KiB (little-endian)
src/decode.hpp     Extracción de campos (opcode, rd, rs1...) e inmediatos I/S/B/U/J
src/simulator.*    Estado arquitectural (PC, x0-x31, memoria) y step(): fetch -> decode -> execute
src/disasm.cpp     Desensamblador
src/main.cpp       Interfaz de comandos (REPL) y carga desde la línea de comandos
tests/             Programas de prueba en ensamblador y sus .bin/.elf
```

- **Estado.** `uint32_t pc`, `uint32_t regs[32]` y un objeto `Memory`. Todas las escrituras a registros pasan por `set_reg`, que ignora `x0` para que siempre valga 0.
- **Memoria.** No se reservan 4 GiB: se usa un `unordered_map` de páginas de 4 KiB que se crean recién cuando se escribe en ellas (leer una página que no existe da 0). Así el address space completo de 32 bits funciona, por ejemplo el stack en `0x10000` y los datos en `0x1000`. `read32/write32` arman y separan las palabras byte por byte en orden little-endian.
- **Ciclo de una instrucción** (`Simulator::step`):
  1. *Fetch:* `ins = mem.read32(pc)`.
  2. *Decode:* se sacan `opcode`, `rd`, `funct3`, `rs1`, `rs2`, `funct7` y el inmediato según el formato. La extensión de signo se hace casteando a `int32_t` y desplazando a la derecha (shift aritmético).
  3. *Execute:* un `switch` por opcode y luego por `funct3`/`funct7`. Las comparaciones con signo (`slt`, `blt`, `bge`, `srai`...) castean a `int32_t`; las sin signo (`sltu`, `bltu`, `bgeu`, `srli`) usan `uint32_t`.
  4. *PC:* `next_pc` empieza en `pc + 4`; los saltos y branches tomados lo cambian.
- **Fin del programa.** Los programas bare-metal terminan con un bucle sobre sí mismos (`j .`, `done: beq x2, x2, done`). Si una instrucción salta a su propia dirección se toma como fin del programa. También termina con `ecall` exit (10) o exit2 (17).

## Pruebas

`tests/build.sh` ensambla los `.s` con clang/LLVM (`--target=riscv32 -march=rv32i`) y genera el `.elf` y el `.bin` crudo. También se pueden sacar de CPUlator como explica el anexo B.

| Programa | Resultado esperado | Resultado en rvsim |
|---|---|---|
| `riscvtest` | 25 (0x19) en la dirección 100 (0x64) | `0x64: 19 00 00 00`, 19 instrucciones |
| `quicksort` | {1, 2, 3, 4, 6, 8, 9} en 0x1000 | ordenado, 610 instrucciones |
| `simetrico` | `a0 = 1` | `a0 = 1`, 153 instrucciones |
| `todas` (propio) | prueba las 37 instrucciones, casos borde y ecalls | imprime `OK`, sale con código 0 |

`todas.s` revisa casos donde es fácil equivocarse: extensión de signo de `lb`/`lh` frente a `lbu`/`lhu`, orden little-endian, `srai` frente a `srli`, `slt` frente a `sltu` con negativos, shifts por registro con cantidad ≥ 32 (solo cuentan 5 bits), `jalr` con offset, escritura a `x0` y offsets negativos. Si una prueba falla, imprime su número y sale con ese código.
