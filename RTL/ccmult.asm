    ld a,h
    xor d
    sra a             ; arithmetic shift → sign in bit 0
    ld b,a            ; B = 0 if positive, FFh if negative

    ; ---- Make HL absolute ----
    ld a,h
    sra h             ; replicate sign into all bits
    xor h
    sub h
    ld h,a

    ld a,l
    xor h             ; apply same mask
    sub h
    ld l,a

    ; ---- Make DE absolute ----
    ld a,d
    sra d
    xor d
    sub d
    ld d,a

    ld a,e
    xor d
    sub d
    ld e,a

    call ccumult

    ; ---- Apply final sign correction without branch ----
    ld a,b
    or a
    jr z,.nofix

    ld a,h
    cpl
    ld h,a
    ld a,l
    cpl
    ld l,a
    inc hl
.nofix