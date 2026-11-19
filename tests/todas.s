# todas.s: prueba propia que usa las 37 instrucciones de RV32I y algunas ecall.
# Cada CHECK compara un registro con el valor esperado; si falla salta a "fail"
# con el numero de prueba en s11. Si todo sale bien imprime "OK" y termina con 0.
.macro CHECK reg, val, num
    li   s11, \num
    li   t6, \val
    bne  \reg, t6, fail
.endm

.global _start
_start:
    li   sp, 0x10000
# --- tipo U ---
    lui  t0, 0x12345
    CHECK t0, 0x12345000, 1
    auipc t0, 1                # t0 = pc + 0x1000
    auipc t1, 0                # t1 = (pc + 4)
    sub  t0, t0, t1
    CHECK t0, 0xFFC, 2
# --- aritmetica con inmediato ---
    li   t0, -8
    addi t1, t0, 3
    CHECK t1, -5, 3
    slti t1, t0, 0             # -8 < 0 con signo
    CHECK t1, 1, 4
    sltiu t1, t0, 5            # 0xFFFFFFF8 < 5 sin signo? no
    CHECK t1, 0, 5
    li   t2, 5
    sltiu t1, t2, -1           # 5 < 0xFFFFFFFF sin signo
    CHECK t1, 1, 6
    xori t1, t0, -1            # not(-8) = 7
    CHECK t1, 7, 7
    ori  t1, t2, 0x30
    CHECK t1, 0x35, 8
    andi t1, t0, 0xFF
    CHECK t1, 0xF8, 9
    slli t1, t2, 4
    CHECK t1, 0x50, 10
    srli t1, t0, 28
    CHECK t1, 0xF, 11
    srai t1, t0, 1
    CHECK t1, -4, 12
# --- registro-registro ---
    li   t3, 33                # shift de 33 -> solo cuentan 5 bits -> 1
    add  t1, t0, t2
    CHECK t1, -3, 13
    sub  t1, t2, t0
    CHECK t1, 13, 14
    sll  t1, t2, t3
    CHECK t1, 10, 15
    slt  t1, t0, t2
    CHECK t1, 1, 16
    sltu t1, t0, t2
    CHECK t1, 0, 17
    xor  t1, t0, t2
    CHECK t1, -3, 18
    srl  t1, t0, t3
    CHECK t1, 0x7FFFFFFC, 19
    sra  t1, t0, t3
    CHECK t1, -4, 20
    or   t1, t0, t2
    CHECK t1, -3, 21
    and  t1, t0, t2
    CHECK t1, 0, 22
# --- x0 siempre es 0 ---
    addi zero, zero, 5
    CHECK zero, 0, 23
# --- stores y loads (little-endian, extension de signo) ---
    li   a0, 0x3000
    li   t0, 0x80FF7F01
    sw   t0, 0(a0)
    lw   t1, 0(a0)
    CHECK t1, 0x80FF7F01, 24
    lbu  t1, 0(a0)             # byte menos significativo va primero
    CHECK t1, 0x01, 25
    lb   t1, 2(a0)
    CHECK t1, -1, 26
    lbu  t1, 2(a0)
    CHECK t1, 0xFF, 27
    lh   t1, 2(a0)
    CHECK t1, 0xFFFF80FF, 28
    lhu  t1, 2(a0)
    CHECK t1, 0x80FF, 29
    lh   t1, 0(a0)
    CHECK t1, 0x7F01, 30
    li   t2, 0xABCD
    sh   t2, 4(a0)
    li   t2, 0x99
    sb   t2, 6(a0)
    sb   zero, 7(a0)
    lw   t1, 4(a0)
    CHECK t1, 0x0099ABCD, 31
    sw   t0, -4(a0)            # offset negativo
    lw   t1, -4(a0)
    CHECK t1, 0x80FF7F01, 32
# --- branches ---
    li   t0, -1
    li   t1, 1
    li   s11, 33
    beq  t0, t1, fail
    bne  t0, t0, fail
    blt  t1, t0, fail          # 1 < -1 ? no
    bge  t0, t1, fail          # -1 >= 1 ? no
    bltu t0, t1, fail          # 0xFFFFFFFF < 1 ? no
    bgeu t1, t0, fail          # 1 >= 0xFFFFFFFF ? no
    li   s11, 34
    blt  t0, t1, b1
    j    fail
b1: bge  t1, t0, b2
    j    fail
b2: bltu t1, t0, b3
    j    fail
b3: bgeu t0, t1, b4
    j    fail
b4: bge  t0, t0, b5            # iguales -> taken
    j    fail
b5: beq  t1, t1, b6
    j    fail
b6: bne  t0, t1, b7
    j    fail
b7:
# --- jal / jalr ---
    jal  ra, func
    CHECK a1, 77, 35
    la   t0, salto
    jalr t2, 4(t0)             # salta a salto+4, t2 = direccion de retorno
ret_aqui:
    j    fail
salto:
    j    fail                  # salto+0: no debe ejecutarse
    la   t3, ret_aqui          # salto+4: aqui cae el jalr
    li   s11, 36
    bne  t2, t3, fail          # t2 debe ser la instruccion siguiente al jalr
# --- fence es nop ---
    fence
# --- todo OK ---
    la   a0, msg_ok
    li   a7, 4                 # print string
    ecall
    li   a0, 37
    li   a7, 1                 # print int
    ecall
    li   a0, 10
    li   a7, 11                # print char ('\n')
    ecall
    li   a0, 0
    li   a7, 17                # exit2(0)
    ecall
fail:
    la   a0, msg_fail
    li   a7, 4
    ecall
    mv   a0, s11
    li   a7, 1
    ecall
    li   a0, 10
    li   a7, 11
    ecall
    mv   a0, s11
    li   a7, 17                # exit2(numero de prueba)
    ecall
func:
    li   a1, 77
    ret
msg_ok:   .asciz "OK: todas las pruebas pasaron. Instrucciones probadas: "
msg_fail: .asciz "FALLO en la prueba "
