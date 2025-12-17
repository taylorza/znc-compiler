 push hl
 ld bc,0
 xor a
 cpir
 jr nz,.l2
 dec hl
 dec hl
 ld a,(hl)
 or $80
 ld (hl),a
.l2
 pop hl