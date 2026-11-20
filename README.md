# rvsim

Simulador de RISC-V (RV32I) por consola, hecho en C++ para la Tarea 5 de Arquitectura de Computadores (CS3051, 2026-II).

Carga un programa ya compilado (un .bin crudo sacado de CPUlator o un .elf) y lo ejecuta instrucción por instrucción. En cualquier momento se puede ver el PC, los registros x0 a x31 y la memoria.

## Compilar

Lo trabajé en CLion: se abre la carpeta del proyecto y se le da a Build. También se puede compilar desde la terminal con CMake:

```
cmake -B build
cmake --build build
```

Si usas el MinGW que trae CLion y el programa se cierra sin mostrar nada al correrlo desde PowerShell, es porque no encuentra las DLL de MinGW. Se arregla agregándolas al PATH:

```
$env:Path = "C:\ruta\a\CLion\bin\mingw\bin;" + $env:Path
```

## Uso

```
rvsim.exe tests\riscvtest.bin
```

El programa se carga desde la dirección 0 y queda esperando comandos:

```
step [n]          ejecuta 1 instrucción (o n)
run               ejecuta hasta el final o hasta un breakpoint
pc                muestra el PC
regs [x5 x10..]   muestra todos los registros o los que le pidas
mem ini [fin]     muestra los bytes de memoria entre ini y fin
memw dir [n]      muestra n palabras de 32 bits desde dir
dis [dir] [n]     desensambla n instrucciones
break dir         pone un breakpoint
delete dir        quita un breakpoint
reset             vuelve a cargar el programa
exit              sale
```

Por ejemplo, con el riscvtest:

```
> step
0x00000000:  0x00500113   addi x2, x0, 5    ; x2 = 0x00000005
> run
Programa terminado: bucle infinito en 0x00000050 (19 instrucciones ejecutadas)
> mem 0x64 0x67
Memoria (0x00000064 - 0x00000067):
  0x00000064: 19 00 00 00
```

El programa se da por terminado cuando una instrucción salta a sí misma (como el `j .` del final de los programas de prueba) o con un `ecall` de exit.

## Qué soporta

- Las 37 instrucciones de RV32I que pide el enunciado.
- Memoria de 32 bits en little-endian.
- Archivos .bin crudos y .elf.
- Algunas `ecall` de CPUlator (el número de servicio va en x17): imprimir entero (1), imprimir string (4), leer entero (5), exit (10), imprimir caracter (11), exit con código (17) e imprimir en hex (34).
- Breakpoints y desensamblador.

## Código

- `src/memory.hpp`: la memoria.
- `src/decode.hpp`: saca los campos de la instrucción y arma los inmediatos de cada formato.
- `src/simulator.cpp`: el estado del procesador y `step()`, que hace fetch, decode y execute de una instrucción.
- `src/disasm.cpp`: el desensamblador.
- `src/main.cpp`: los comandos de la consola.

## Pruebas

En `tests/` están los tres programas del enunciado y dos más que hice yo:

- `riscvtest`: si todo va bien deja 25 en la dirección 100.
- `quicksort`: ordena {6, 4, 3, 2, 1, 8, 9} en 0x1000.
- `simetrico`: termina con x10 = 1.
- `todas`: prueba las 37 instrucciones, incluidos casos como la extensión de signo en `lb`/`lh` o las comparaciones con y sin signo. Imprime OK si todo pasa.
- `suma`: pide un número n por teclado e imprime 1 + 2 + ... + n.

Los .bin se pueden sacar de CPUlator como explica el anexo B, o generar con `tests/build.sh` si tienes clang.
