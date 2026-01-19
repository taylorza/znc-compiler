 ; IX + (varg_count * 2) + 6
 ld l,(ix+4)
 ld h,0
 add hl,hl 
 add hl,6  
 ld e,ixl
 ld d,ixh
 add hl,de