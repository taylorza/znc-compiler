; ccfxdiv
; Fixed-point divide: DE / HL -> HL  (12.4 signed format)
; Entry: DE = dividend (12.4 signed), HL = divisor (12.4 signed)
; Handles negative operands: extract signs, divide magnitudes, restore sign.
 ld a,d
 xor h            ; bit 7 of A = sign of result (1 if operands have different signs)
 push af          ; save result sign in S flag

 ; absolute value of DE
 bit 7,d
 jr z,.de_pos
 xor a
 sub e
 ld e,a
 sbc a,a
 sub d
 ld d,a
.de_pos:
 ; absolute value of HL
 bit 7,h
 jr z,.hl_pos
 xor a
 sub l
 ld l,a
 sbc a,a
 sub h
 ld h,a
.hl_pos:
 ld b,h
 ld c,l           ; BC = divisor (preserved throughout)
 ld hl,0          ; HL = remainder = 0
 ld a,16          ; Loop1: process 16 bits of dividend (DE)

.l1:
 sla e
 rl d             ; shift DE left; MSB of D feeds into carry -> HL
 adc hl,hl        ; shift remainder left, bring in that bit
 jr nc,.ns1
 or a
 sbc hl,bc        ; remainder overflowed 16-bit so definitely >= BC; subtract
 jp .sq1
.ns1:
 sbc hl,bc        ; try subtracting divisor
 jr c,.nxt1
.sq1:
 inc e            ; quotient bit = 1
 jr .cnt1
.nxt1:
 add hl,bc        ; restore remainder (subtraction failed)
.cnt1:
 dec a
 jr nz,.l1

 ; Loop2: 4 more iterations for the Q4 fractional extension
 ; No overflow byte tracking; we only care about low 16 bits of quotient.
 ld a,4

.l2:
 sla e
 rl d             ; shift DE left; ignore carry out of D (overflow beyond 16 bits)
 add hl,hl        ; shift remainder left (zero bits shifting in from dividend)
 jr nc,.ns2
 sbc hl,bc
 jp .sq2
.ns2:
 sbc hl,bc
 jr c,.nxt2
.sq2:
 inc e
 jr .cnt2
.nxt2:
 add hl,bc
.cnt2:
 dec a
 jr nz,.l2

 ; Round to nearest: if 2*remainder >= divisor (BC), add 1 to quotient
 add hl,hl        ; double the remainder
 jr c,.rup        ; 2*remainder overflowed -> definitely >= BC
 sbc hl,bc        ; carry=0 here; borrow means 2*remainder < BC, no round
 jr c,.done
.rup:
 inc e
 jr nz,.done
 inc d
.done:
 ex de,hl         ; result (12.4 magnitude quotient) into HL

 ; restore sign
 pop af           ; restore result sign (S flag = bit 7 of saved A)
 ret p            ; positive result (same signs), done

 ; negate HL (result is negative: operands had different signs)
 xor a
 sub l
 ld l,a
 sbc a,a
 sub h
 ld h,a
