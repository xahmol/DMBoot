; GEOS 128 restart from REU image
;
; By Bart van Leeuwen in 2021
;
; This software is in the public domain.
;
; Adapted from ACME original to CA65 syntax by Xander Mol

.export _startgeos
.export _errorcode

.segment	"OVERLAY5"

_startgeos:
        sei
        lda     $ff00          ; load old memory configuration to enable safe return to C
        sta     memoryconfig   ; save old configuration in variable
        lda     #$00
        sta     $ff00          ; select bank 15: kernal, monitor, editor, basic roms enabled, io enabled.
        lda     $df06          ; REU bank register
        pha                    ; store it, we may need to restore it later
        lda     #$07           ; we are going to set the low 3 bits.
        sta     $df06          
        lda     $df06          ; and see what value the register (or whatever it is, we don't know yet) holds..
        cmp     #$ff           ; the bank register forces the high 5 bits to 1, so after setting the low 3 bits to 1, we should get 255 as a result
        bne     reuerror      ; no REU pressent.
        lda     #$00           ; test clearing all bits in the REU bank register
        sta     $df06
        lda     $df06          ; and see what value the register holds
        cmp     #$f8           ; note that the top 5 bits of this register are always 1, regardless of the value stored. This behavior is unique to the REU BANK register
        bne     reuerror      ; and combined with previous check lets us determine if we really have an REU 
        pla                    ; we don't need this anymore.
        lda     $D506          ; shared ram and dma target register
        ora     #$47           ; enable 16k shared ram at bottom and top of memory (note: $0000-$3fff come from ram block 0, $c000-$ffff come from ram block 1)
                               ; also, set DMA target to ram block 1 for the REU to ram copy (and yes, this also tells VIC2 to get its data from ram block 1..)
        sta     $D506          ; and store this configuration.
        lda     #$7E           ; select ram block 1 (note: our code actually resides in ram block 0 but will remain visible as it is in the low shared ram area), disable all roms, keep IO enabled.
        sta     $FF00
        ldx     #(resetcode_end - resetcode) - 1
:
        lda     resetcode,x   ; copy GEOS reset handler to $03e4 (GEOS will take care of setting up the pointer to this handler, but won't put the handler in place unless you do a cold start from disk)
        sta     $03e4,x
        dex
        bpl     :-
        lda     $D030
        and     #$FE           ; 1mhz mode for DMA, system will often crash after completion of DMA when in 2mhz mode (50% chance of starting at the wrong phase of the clock)
        sta     $D030
        ldy     #$08
:       lda     reudata,y     ; setup REU for copying bootloader to ram
        sta     $DF01,y
        dey
        bpl     :-
        ldx     #$08
:
        lda     $c006,x        ; does this look like a GEOS rboot loader?
        cmp     sig,x
        bne     sigerror
        dex
        bpl     :-
        jmp     $C000          ; yes it did, call it, it will take care of everything from here.

; print message and exit, note this must be called with a default shared ram config and with a bank 15 memory config
reuerror:
        pla
        sta     $df06          ; probably does nothing, but.. just in case something mapped some ram here, we restore the original content...
        lda     #$01
        sta     _errorcode     ; exit with error code 1 for REU not enabled
        cli
        rts    

; we didn't find something which looks like a GEOS rboot loader. Note we first need to setup a 'sane' memory config so we can call kernal functions and return to basic.
sigerror:
        ldx     memoryconfig   ; load old memory config back
        stx     $ff00          ; set memory map to original for safe return to C, this must be done before restoring the default shared ram config
        lda     #$04           ; set DMA target and shared ram config to ram block 0 for DMA and 1k shared ram at bottom of memory
        sta     $d506
        lda     #$02
        sta     _errorcode     ; exit with error code 2 for unvalid GEOS RAM image
        cli
        rts

; a GEOS rboot loader should have this at offset $0006
sig:
.byte   "geos boot"       

reudata:
.byte   $91, $00, $c0, $40, $bc 
.byte   $00, $80, $00, $00, $00

resetcode:
        lda     #$7e
        sta     $ff00
        jmp     $c000
resetcode_end:

memoryconfig:  .byte   0
_errorcode:     .byte   0

