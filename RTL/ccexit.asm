 push af
 push hl
 
 ld a,(ccexit.cnt)
 
.next
 dec a
 jp m,.ret

 push af
 add a
 ld hl,__exitfn
 add hl,a

 ld a,(hl)
 inc hl
 ld h,(hl)
 ld l,a
 
 push .step
 jp (hl)

.step
  pop af
  jp .next

.ret
 pop hl
 pop af
 jr +_
ccexit.cnt db 0
_