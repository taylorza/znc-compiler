 ld a,e
 sub l
 ld e,a
 ld a,d
 sbc a,h
 ld hl,1
 jp m,cccmp1
 or e
 ret
cccmp1
 or e
 scf