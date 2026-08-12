 ld a, d
 xor h
 ex af, af'
 bit 7,d
 jr z,.divA
 
 ld a,e ; DE = -DE
 cpl
 ld e,a
 ld a,d
 cpl
 ld d,a
 inc de
 
.divA
 bit 7,h
 call nz,ccneg

 call ccudiv

 ex af,af'
 or a
 ret p
 jp ccneg