 pop de
 ex de,hl
.case
 ld a,(hl)
 inc hl
 cp e
 jr nz,.skip3
 ld a,(hl)
 inc hl
 cp d        
 jr nz,.skip2
 ld a,(hl)
 inc hl
 ld h,(hl)
 ld l,a
 jp (hl)
.skip3
 inc hl
.skip2
 inc hl
 inc hl
 djnz .case
 push hl