 ld a,d
 cp h
 jp nz,ccucmp1
 ld a,e
 cp l
ccucmp1
 ld hl,1