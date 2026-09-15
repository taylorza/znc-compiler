    ld b, 0             ; Initialize sign tracker in B

    ; Check sign of HL
    bit 7, h
    jr z, .chk_de
    inc b               ; HL is negative, toggle sign tracker

    ; Negate HL (HL = 0 - HL)
    xor a
    sub l
    ld l, a
    sbc a, h
    sub a, l
    ld h, a

.chk_de:
    ; Check sign of DE
    bit 7, d
    jr z, .mult
    inc b               ; DE is negative, toggle sign tracker

    ; Negate DE (DE = 0 - DE)
    xor a
    sub e
    ld e, a
    sbc a, d
    sub a, e
    ld d, a

.mult:
    call ccumult        ; Core math routine (Destroys A, C, DE, HL; Preserves B)
    
    ; Test if the result needs to be negative
    bit 0, b            ; If B is odd (1), result must be negative
    ret z               ; If B is even (0 or 2), result is already correct

    ; Negate HL to restore the negative sign
    xor a
    sub l
    ld l, a
    sbc a, h
    sub a, l
    ld h, a