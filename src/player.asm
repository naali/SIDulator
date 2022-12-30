* = $2000 "Dummy player"

player: {
        lda #0
        sta $10
        sta $11
        sta $12
        sta $13

        sta $20

        lda #$1     // subtune
        jsr $0800   // music init

    loop:
        jsr $0803   // music player
        jsr incFrameCounter
        jmp loop

incFrameCounter: {
        clc
        lda #$1
        adc $10
        sta $10

        lda $11
        adc #$0
        sta $11

        lda $12
        adc #$0
        sta $12

        lda $13
        adc #$0
        sta $13

        lda $20
        eor #$FF
        sta $20
        rts
    }
}