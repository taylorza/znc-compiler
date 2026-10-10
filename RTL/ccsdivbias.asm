 ld a,d
 rla
 sbc a,a
 ld c,a
 ld a,l
 and c
 ld l,a
 ld a,h
 and c
 ld h,a
 add hl,de
 ex de,hl
