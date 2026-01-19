 ld bc,0
.putc
 ld a,(hl)
 or a
 jr z,.done
 rst $10
 inc hl
 inc bc
 jr .putc
.done
 ld l,c
 ld h,b