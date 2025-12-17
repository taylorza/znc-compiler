 ld a, (hl)
 or a
 jr z, .done
 rst $10
 inc hl
 jr puts
.done