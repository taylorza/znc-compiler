; Fixed-point multiply: DE * HL -> HL  (12.4 signed format)
; DE = left operand (12.4 signed), HL = right operand (12.4 signed)
; Handles negative operands: extract signs, multiply magnitudes, restore sign.
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
 ld c,l           ; BC = |right operand|, DE = |left operand|
 call ccmul32
 ; BCHL = 32-bit unsigned product, shift right 4 to get Q4 magnitude in HL
 srl b
 rr c
 rr h
 rr l
 srl b
 rr c
 rr h
 rr l
 srl b
 rr c
 rr h
 rr l
 srl b
 rr c
 rr h
 rr l
 ; HL = magnitude of result in Q4
 pop af           ; restore result sign (S flag = bit 7 of saved A)
 ret p            ; positive result (same signs), done
 ; negate HL (result is negative: operands had different signs)
 xor a
 sub l
 ld l,a
 sbc a,a
 sub h
 ld h,a
 ret

; BC * DE -> BC:HL (32-bit unsigned)
; Shifts DE:HL left 16 times; D's MSB each iteration is the current multiplier bit.
; When set, add multiplicand (BC) to HL; carry from that goes into DE.
; Clobbers: AF, DE
ccmul32:
 ld   hl,0
 ld   a,16
.loop:
 add  hl,hl
 rl   e
 rl   d            ; DE:HL <<= 1; D's old bit 7 -> carry = current multiplier bit
 jr   nc,.no_add
 add  hl,bc        ; add multiplicand to low word
 jr   nc,.no_add
 inc  de            ; carry from low-word addition into high word
.no_add:
 dec  a
 jr   nz,.loop
 ld   b,d
 ld   c,e           ; BC = high 16 bits of product
 ret