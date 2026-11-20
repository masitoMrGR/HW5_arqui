# todas.s: prueba las 37 instrucciones de RV32I.
# CHECK compara un registro con el valor esperado. Si no coincide salta a fail
# con el numero de prueba en x27. Si todo sale bien imprime OK y sale con codigo 0.
.macro CHECK reg, val, num
    li   x27, \num
    li   x31, \val
    bne  \reg, x31, fail
.endm

.global _start
_start:
    li   x2, 0x10000

# lui, auipc
    lui  x5, 0x12345
    CHECK x5, 0x12345000, 1
    auipc x5, 1                 # x5 = pc + 0x1000
    auipc x6, 0                 # x6 = pc + 4
    sub  x5, x5, x6
    CHECK x5, 0xFFC, 2

# operaciones con inmediato (x5 = -8, x7 = 5)
    li   x5, -8
    li   x7, 5
    addi x6, x5, 3
    CHECK x6, -5, 3
    slti x6, x5, 0              # -8 < 0 con signo
    CHECK x6, 1, 4
    sltiu x6, x5, 5             # 0xFFFFFFF8 < 5 sin signo: no
    CHECK x6, 0, 5
    sltiu x6, x7, -1            # 5 < 0xFFFFFFFF sin signo: si
    CHECK x6, 1, 6
    xori x6, x5, -1
    CHECK x6, 7, 7
    ori  x6, x7, 0x30
    CHECK x6, 0x35, 8
    andi x6, x5, 0xFF
    CHECK x6, 0xF8, 9
    slli x6, x7, 4
    CHECK x6, 0x50, 10
    srli x6, x5, 28
    CHECK x6, 0xF, 11
    srai x6, x5, 1
    CHECK x6, -4, 12

# operaciones entre registros (x28 = 33: en los shifts solo cuentan 5 bits, o sea 1)
    li   x28, 33
    add  x6, x5, x7
    CHECK x6, -3, 13
    sub  x6, x7, x5
    CHECK x6, 13, 14
    sll  x6, x7, x28
    CHECK x6, 10, 15
    slt  x6, x5, x7
    CHECK x6, 1, 16
    sltu x6, x5, x7
    CHECK x6, 0, 17
    xor  x6, x5, x7
    CHECK x6, -3, 18
    srl  x6, x5, x28
    CHECK x6, 0x7FFFFFFC, 19
    sra  x6, x5, x28
    CHECK x6, -4, 20
    or   x6, x5, x7
    CHECK x6, -3, 21
    and  x6, x5, x7
    CHECK x6, 0, 22

# x0 siempre vale 0
    addi x0, x0, 5
    CHECK x0, 0, 23

# stores y loads (little-endian y extension de signo)
    li   x10, 0x3000
    li   x5, 0x80FF7F01
    sw   x5, 0(x10)
    lw   x6, 0(x10)
    CHECK x6, 0x80FF7F01, 24
    lbu  x6, 0(x10)             # el byte bajo esta en la direccion mas baja
    CHECK x6, 0x01, 25
    lb   x6, 2(x10)
    CHECK x6, -1, 26
    lbu  x6, 2(x10)
    CHECK x6, 0xFF, 27
    lh   x6, 2(x10)
    CHECK x6, 0xFFFF80FF, 28
    lhu  x6, 2(x10)
    CHECK x6, 0x80FF, 29
    lh   x6, 0(x10)
    CHECK x6, 0x7F01, 30
    li   x7, 0xABCD
    sh   x7, 4(x10)
    li   x7, 0x99
    sb   x7, 6(x10)
    sb   x0, 7(x10)
    lw   x6, 4(x10)
    CHECK x6, 0x0099ABCD, 31
    sw   x5, -4(x10)            # offset negativo
    lw   x6, -4(x10)
    CHECK x6, 0x80FF7F01, 32

# branches que NO deben saltar (x5 = -1, x6 = 1)
    li   x5, -1
    li   x6, 1
    li   x27, 33
    beq  x5, x6, fail
    bne  x5, x5, fail
    blt  x6, x5, fail
    bge  x5, x6, fail
    bltu x5, x6, fail
    bgeu x6, x5, fail

# branches que SI deben saltar
    li   x27, 34
    blt  x5, x6, b1
    j    fail
b1: bge  x6, x5, b2
    j    fail
b2: bltu x6, x5, b3
    j    fail
b3: bgeu x5, x6, b4
    j    fail
b4: bge  x5, x5, b5
    j    fail
b5: beq  x6, x6, b6
    j    fail
b6: bne  x5, x6, b7
    j    fail
b7:

# jal, jalr
    jal  x1, func
    CHECK x11, 77, 35
    la   x5, salto
    jalr x7, 4(x5)              # salta a salto + 4 y guarda el retorno en x7
ret_aqui:
    j    fail
salto:
    j    fail
    la   x28, ret_aqui
    li   x27, 36
    bne  x7, x28, fail          # x7 debe apuntar a la instruccion despues del jalr

# todo OK
    la   x10, msg_ok
    li   x17, 4
    ecall
    li   x10, 0
    li   x17, 17                # exit con codigo 0
    ecall

fail:
    la   x10, msg_fail
    li   x17, 4
    ecall
    mv   x10, x27
    li   x17, 1
    ecall
    li   x10, 10
    li   x17, 11
    ecall
    mv   x10, x27
    li   x17, 17                # exit con el numero de la prueba que fallo
    ecall

func:
    li   x11, 77
    ret

msg_ok:   .asciz "OK: pasaron las pruebas de las 37 instrucciones\n"
msg_fail: .asciz "FALLO la prueba "
