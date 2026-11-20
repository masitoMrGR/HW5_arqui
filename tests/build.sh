#!/bin/sh
# Ensambla los .s de esta carpeta a .elf y .bin (necesita clang y LLVM).
# Igual que CPUlator, "call" se optimiza a un solo jal.
# Tambien se pueden obtener desde CPUlator (anexo B del enunciado).
set -e
cd "$(dirname "$0")"
for s in *.s; do
    n="${s%.s}"
    clang --target=riscv32 -march=rv32i -c "$s" -o "$n.o"
    ld.lld -m elf32lriscv -Ttext=0 -e 0 "$n.o" -o "$n.elf"
    llvm-objcopy -O binary "$n.elf" "$n.bin"
    rm "$n.o"
done
