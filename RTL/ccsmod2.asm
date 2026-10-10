 push hl
 call ccsdivbias
 pop bc
 ld a,b
 cpl
 and d
 ld d,a
 ld a,c
 cpl
 and e
 ld e,a
 xor a
 sbc hl,de
