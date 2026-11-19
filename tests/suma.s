# suma.s: pide un numero n por teclado e imprime 1 + 2 + ... + n.
# Sirve para mostrar las ecall de entrada/salida y un bucle.
.global _start
_start:
    la   a0, msg_pedir
    li   a7, 4              # print string
    ecall
    li   a7, 5              # read int -> a0
    ecall
    mv   t0, a0             # t0 = n
    li   t1, 0              # t1 = suma
    li   t2, 1              # t2 = i
loop:
    blt  t0, t2, fin        # si n < i, terminamos
    add  t1, t1, t2         # suma += i
    addi t2, t2, 1          # i++
    j    loop
fin:
    la   a0, msg_res
    li   a7, 4
    ecall
    mv   a0, t1
    li   a7, 1              # print int
    ecall
    li   a0, 10
    li   a7, 11             # print char '\n'
    ecall
    li   a7, 10             # exit
    ecall
msg_pedir: .asciz "Ingresa un numero: "
msg_res:   .asciz "La suma de 1 a n es: "
