 ex de, hl
 ld a, h
 ld c, l
 ld hl, 0
 ld b, 16
.loop
 rl c
 rla
 adc hl, hl
 sbc hl, de
 jp nc, .norestr
 add hl, de
.norestr
 ccf
 djnz .loop
 rl c
 rla
 ex de, hl
 ld h, a
 ld l, c