#ifndef ESXBASE__
const byte ESXBASE__=1;

const byte F_ATTR_RO = 0x01;
const byte F_ATTR_HID = 0x02;
const byte F_ATTR_SYS = 0x04;
const byte F_ATTR_VOL = 0x08;
const byte F_ATTR_DIR = 0x10;
const byte F_ATTR_ARC = 0x20;

byte errno=0;

__asm__ {
  jr esx_r_skip
esx_err
  ld (_errno),a
  ld hl,0
  ret
esx_r_skip
}

byte fclose(byte handle) __znccall(1) __asm__ {
  pop af    ; Return address
  pop bc    ; pop handle in C
  push af   ; restore return address

  xor a
  ld (_errno),a  
  
  ld a,c    ; handle in A
   
  rst 0x08
  db 0x9b ;F_CLOSE
  ld hl,1
  ret nc

  jp esx_err
}

#endif // ESXBASE__