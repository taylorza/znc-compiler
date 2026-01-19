 ld e,(hl)
 inc hl
 ld d,(hl)          ; DE - arg address
 dec hl
 dec de
 dec de             ; DE - next arg
 ld (hl),e
 inc hl
 ld (hl),d          ; Update va_list
 ex de,hl
 ld e,(hl)
 inc hl
 ld d,(hl)
 ex de,hl