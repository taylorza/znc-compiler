 ld de,_args
 ld b,79
 xor a
 ld (.q),a
.l1
 ld a, (hl)
 or a
 jr z,.l2
 cp """"
 jr z,.l3
 ld c,a
 ld a,(.q)
 dec a
 ld a,c
 jr z,.l4
 cp ":"
 jr z,.l2
 cp 13
 jr z,.l2
.l4
 ld (de), a
 inc hl
 inc de
 dec b
 jr nz,.l1
.l2
 xor a
 ld (de), a
 ret
.l3
  ld b,a
  ld a,(.q)
  xor 1
  ld (.q), a
  ld a,b
  jr .l4
.q db 0