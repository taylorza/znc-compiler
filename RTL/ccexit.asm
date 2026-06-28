 push af
 push hl
 ld hl,(__exitfn)
 ld a,l
 or h
 jr z,.ret
 push .ret
 jp (hl)
.ret
 pop hl
 pop af