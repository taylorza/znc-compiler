 ld de,_args
 ld bc,79
.l1
 ld a, (hl)
 or a
 jr z,.l2
 cp ":"
 jr z,.l2
 cp 13
 jr z,.l2
 ld (de), a
 inc hl
 inc de
 dec bc
 ld a, b
 or c
 jr nz,.l1
.l2
 xor a
 ld (de), a