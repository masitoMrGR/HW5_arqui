# suma.s: pide un numero n por teclado e imprime 1 + 2 + ... + n
.global _start
_start:
    la   x10, msg_pedir
    li   x17, 4             # imprimir string
    ecall
    li   x17, 5             # leer entero -> x10
    ecall
    mv   x5, x10            # x5 = n
    li   x6, 0              # x6 = suma
    li   x7, 1              # x7 = i
loop:
    blt  x5, x7, fin        # si n < i, termina
    add  x6, x6, x7         # suma += i
    addi x7, x7, 1          # i++
    j    loop
fin:
    la   x10, msg_res
    li   x17, 4
    ecall
    mv   x10, x6
    li   x17, 1             # imprimir entero
    ecall
    li   x10, 10
    li   x17, 11            # imprimir '\n'
    ecall
    li   x17, 10            # exit
    ecall
msg_pedir: .asciz "Ingresa un numero: "
msg_res:   .asciz "La suma de 1 a n es: "
