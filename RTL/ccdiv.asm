 ld a, d
 xor h
 ex af, af'
 bit 7,d
 
 push de
 call nz, ccdeneg
 
.divA
 bit 7,h
 call nz,ccneg

 call ccudiv

 pop bc
 bit 7,b
 call nz,ccdeneg

 ex af,af'
 or a
 ret p
 jp ccneg