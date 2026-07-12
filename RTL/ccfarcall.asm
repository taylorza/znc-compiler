; ccfarcall — far-call trampoline for banked code
;
; Caller protocol:
;   ld a, <page>          ; page number to map (or bank index for DOT builds)
;   ld de, <target_addr>  ; address of the function inside the banked page
;   call ccfarcall
;
; The target function executes as if called directly — its stack frame is
; identical to a plain CALL.  HL is not modified by this trampoline and
; carries the return value.  BC, DE, AF are caller-saved per ZNC convention.
;
; Trampoline stack layout (per nested call, grows downward from .pagestk):
;   [old_page (2 bytes, only high byte meaningful)] [real_return_addr (2 bytes)]
;
_
  pop hl                ; HL = ret addr from CALL
  
  ; For DOT builds: map the bank index (in A) to the allocated page number.
  if __DOT=1
    exx                 ; HL' = return address, DE' = target address
    ld hl,__page_map
    add hl,a            ; HL = &__page_map[bank_index]  (ZXN extended: add hl,a)
    ld a,(hl)           ; A = allocated page number
    exx                 ; HL = return address, DE = target address
  endif

  ld (.callersp),sp     ; save caller stack pointer
  ld sp,(.sp)           ; switch to trampoline stack

  push hl               ; save real return address on trampoline stack
  ex de,hl              ; HL = target address

  ; Read the page currently mapped at MMU slot 6 (0xC000), then switch
  ; to the target page.  BC is set once and reused for both the select
  ; write and the data read.
  ld bc,9275            ; NextReg select port
  ld d,84               ; NextReg index: MMU slot 6 (0xC000-0xDFFF)
  out (c),d             ; select NextReg 0x56
  inc b                 ; BC = 0x253B (NextReg data port)
  in d,(c)              ; D = page currently mapped at slot 6
  push de               ; save old page on trampoline stack (only D is meaningful)
  out (c),a             ; switch slot 6 to target page

  ld (.sp),sp           ; update trampoline stack pointer
  ld sp,(.callersp)     ; restore caller stack pointer

  ; Caller stack is now exactly [...args...] — identical to before the CALL.
  ; HL = Target address
  call .callhl          ; push ret addr and jump; target sees [ret | ...args...]

  ; Target returned.  Restore MMU state and relay the return to the original caller.
  ld (.callersp),sp     ; save caller stack pointer
  ld sp,(.sp)           ; switch back to trampoline stack

  pop af                ; A = saved page (D was high byte of pushed DE → pops into A)
  pop de                ; DE = saved real return address

  nextreg 84,a          ; restore MMU slot 6 to the previous page

  ld (.sp),sp         ; reset trampoline stack pointer (ready for next call)
  ld sp,(.callersp)   ; restore caller stack pointer

  push de             ; push real return address onto caller stack
  ret                 ; return to original caller (HL = return value, untouched)

.callhl:
  jp (hl)             ; tail-jump into target function

;.pagestk_bot:
          ds 4*16     ; 64 bytes: supports up to 16 levels of nested far calls
.pagestk:
.sp       dw .pagestk ; trampoline stack pointer (initialised to stack top)
.callersp dw 0        ; holds caller SP during trampoline DI sections