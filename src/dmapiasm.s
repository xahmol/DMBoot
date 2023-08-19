; Device Manager ROM API call helper routines
;
; Based on extapi.txt decsription of the Device Manager ROM by Bart van Leeuwen, 2022

.export _dm_getapiversion_core
.export _dm_getdevicetype_core
.export _dm_gethsidviaapi
.export _dm_sethsidviaapi
.export _dm_run64
.export _dm_apipresent
.export _dm_apiverhigb
.export _dm_apiverlowb
.export _dm_devtype
.export _dm_prgnam
.export _dm_prglen
.export _dm_devid

api_header          = $807b
api_version         = $807e
ext_get_drivetype   = $8083
ext_get_hs_id       = $8086
ext_set_hs_id       = $8089
ext_run128          = $808c
ext_run64           = $808f
SETBNK              = $ff68
SETLFS              = $ffba
SETNAM              = $ffbd

.segment	"CODE"

; Core API helper functions

; ------------------------------------------------------------------------------
_dm_getapiversion_core:
; Functiomn to obtain the DM API version
; Input: None
; Output:   API version numvber in dm_apiverhigb and dm_apiverlowb
;           API version present flag in dm_apipresent (0 is no, 1 is yes)
; ------------------------------------------------------------------------------
    
    jsr setmmu                          ; Safeguard old MMU config and set new one to %00101010        

    ; Check DM jumptable version and only proceed if right version
    lda api_header                      ; Load first ID byte
    cmp #$4e                            ; Compare with expected value 'n'
    bne incorrectversion                ; Jump to incorrect version
    lda api_header+1                    ; Load second ID byte
    cmp #$45                            ; Compare with expected value 'e'
    bne incorrectversion                ; Jump to incorrect version
    lda api_header+2                    ; Load third byte
    cmp #$44                            ; Compare with expected value 'd'
    bne incorrectversion                ; Jump to incorrect version

    ; Set API present flag to 1
    lda #$01;                           ; Load 1 in A
    sta _dm_apipresent                  ; Store in API present flag variable

    ; Load and store major and minor version number bytes
    lda api_version                     ; Load first byte
    sta _dm_apiverlowb                  ; Store first byte in variable
    lda api_version+1                   ; Load second byte
    sta _dm_apiverhigb                  ; Store second byte in variable

incorrectversion:
    jsr restoremmu                      ; Restore MMU config
    rts

; ------------------------------------------------------------------------------
_dm_getdevicetype_core:
; Function to get device type of drive at selected ID
; Input: Device ID to test in dm_devtype
; Output: Device type in dm_devtype:
;         $01 : unknown type
;         $02 : UII drive A (get_drivetype will not return this value)
;         $03 : UII drive B (get_drivetype will not return this value)
;         $04 : sd2iec (recognized by device init message)
;         $05 : microIEC (sd2iec like, recognized by device init message)
;         $06 : printer (yes.. those exist as well, simply recognized by using dev id 4 or 5)
;         $07 : 1526 plotter (recognized by device id...)
;         $08 : UII(+) software IEC (recognized by device init message)
;         $09 : pi1541 (iec browser mode, recognized by device init message)
;         $28 : 1540 (recognized by drive rom signature)
;         $29 : 1541 (recognized by drive rom signature)
;         $46 : 1570 (recognized by drive rom signature)
;         $47 : 1571 (recognized by drive rom signature)
;         $51 ; 1581 (recognized by drive rom signature)
;         $80 : CMD RL type drive (recognized by drive rom signature)
;         $c0 : CMD HD type drive (recognized by drive rom signature)
;         $e0 : CMD FD type drive (recognized by drive rom signature)
;         $f0 : CMD RD type drive (recognized by drive rom signature)
; ------------------------------------------------------------------------------

    jsr setmmu                          ; Safeguard old MMU config and set new one to %00101010  

; Check device ID and obtain type
    lda _dm_devtype                     ; Load device ID to be tested
    jsr ext_get_drivetype               ; Call DM ext_get_drivetype  function
    sta _dm_devtype                     ; Store obtained device type
    
    jsr restoremmu                      ; Restore MMU config
    rts

; ------------------------------------------------------------------------------
_dm_gethsidviaapi:
; Function to set the hyperdrive drive ID to 8
; ------------------------------------------------------------------------------
    jsr setmmu                          ; Safeguard old MMU config and set new one to %00101010  

    ; Set hyperspeed drive ID to 8 via DM API call
    jsr ext_get_hs_id                   ; Call DM ext_get_hs_id function
    sta _dm_devid                       ; Store outcome in variable

    jsr restoremmu                      ; Restore MMU config
    rts

; ------------------------------------------------------------------------------
_dm_sethsidviaapi:
; Function to set the hyperdrive drive ID to 8
; ------------------------------------------------------------------------------
    jsr setmmu                          ; Safeguard old MMU config and set new one to %00101010  

    ; Set hyperspeed drive ID to 8 via DM API call
    lda #$08                            ; Load 8 to A
    jsr ext_set_hs_id                   ; Call DM ext_set_hs_id function

    jsr restoremmu                      ; Restore MMU config
    rts

; ------------------------------------------------------------------------------
_dm_run64:
; Function to run the file pointed at by a call to SETNAM in 64 mode
; Input:    dm_prgnam = filename to load
;           dm_prglen = length of filename string
; ------------------------------------------------------------------------------
    lda #$00                            ; Load 0 for bank 10
    ldx #$00                            ; Load 0 for filename bank 0
    jsr SETBNK                          ; Call to SETBNK kernal function
    lda #$00                            ; Load 0 as logical file number
    ldx _dm_devid                       ; Load device ID
    ldy #$00                            ; Load 0 as secondary ID
    jsr SETLFS                          ; Call to SETLFS kernal function
    lda _dm_prglen                      ; Load string length in A
    ldx #<_dm_prgnam                    ; Load low byte of filenmae string address in X
    ldy #>_dm_prgnam                    ; Load high byte of filenmae string address in Y
    jsr SETNAM                          ; Call to SETNAM kernal function
    jsr setmmu                          ; Safeguard old MMU config and set new one to %00101010    
    jmp ext_run64                       ; Call DM ext_run64 function
    jsr restoremmu                      ; Restore MMU config
    rts

; Generic functions

; ------------------------------------------------------------------------------
setmmu:
; Functiomn to set MMU properly to reach API of DM ROM
; and safeguard present config
; ------------------------------------------------------------------------------
    ; Safeguard old MMU config and set new one to %00101010
    lda $ff00                           ; Load preent MMU config
    sta mmutmp                          ; Store MMU config to temporary variable
    lda #$2a                            ; Load MMU config to access function ROM
    sta $ff00                           ; Store MMU config
    rts

; ------------------------------------------------------------------------------
restoremmu:
; Functiomn to restore MMU to safeguarded config
; ------------------------------------------------------------------------------
    ; Restore MMU config
    lda mmutmp                          ; Load saved MMU config
    sta $ff00                           ; Restore MMU config
    rts

mmutmp:
    .res    1

_dm_apipresent:
    .res    1

_dm_apiverhigb:
    .res    1

_dm_apiverlowb:
    .res    1

_dm_devtype:
    .res    1

_dm_prgnam:
    .res    20

_dm_prglen:
    .res    1

_dm_devid:
    .res    1
