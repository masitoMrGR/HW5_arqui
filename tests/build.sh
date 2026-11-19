#!/bin/sh
# Ensambla los .s de esta carpeta a .elf y .bin (binario crudo desde 0x00000000).
# Requiere clang + ld.lld + llvm-objcopy (o usa CPUlator, ver anexo B del enunciado).
set -e
cd "$(dirname "$0")"
for s in *.s; do
    n="${s%.s}"
    clang --target=riscv32 -march=rv32i -mno-relax -c "$s" -o "$n.o"
    ld.lld -m elf32lriscv -Ttext=0 -e 0 "$n.o" -o "$n.elf"
    llvm-objcopy -O binary "$n.elf" "$n.bin"
    rm "$n.o"
    echo "$n.bin ($(wc -c < "$n.bin") bytes)"
done
