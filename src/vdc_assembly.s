; ====================================================================================
; vdc_core_assembly.s
; Core assembly routines for vdc_core.c
;
; Credits for code and inspiration:
;
; C128 Programmers Reference Guide:
; http://www.zimmers.net/anonftp/pub/cbm/manuals/c128/C128_Programmers_Reference_Guide.pdf
;
; Scott Hutter - VDC Core functions inspiration:
; https://github.com/Commodore64128/vdc_gui/blob/master/src/vdc_core.c
; (used as starting point, but channged to inline assembler for core functions, added VDC wait statements and expanded)
;
; Francesco Sblendorio - Screen Utility:
; https://github.com/xlar54/ultimateii-dos-lib/blob/master/src/samples/screen_utility.c
;
; DevDef: Commodore 128 Assembly - Part 3: The 80-column (8563) chip
; https://devdef.blogspot.com/2018/03/commodore-128-assembly-part-3-80-column.html
;
; Tips and Tricks for C128: VDC
; http://commodore128.mirkosoft.sk/vdc.html
;
; 6502.org: Practical Memory Move Routines
; http://6502.org/source/general/memory_move.html
;
; =====================================================================================


    .export		_VDC_ReadRegister_core
	.export		_VDC_WriteRegister_core
	.export		_VDC_Poke_core
	.export		_VDC_Peek_core
	.export		_VDC_DetectVDCMemSize_core
	.export		_VDC_SetExtendedVDCMemSize
	.export		_VDC_DefaultVDCMemSize
	.export		_VDC_MemCopy_core
	.export		_VDC_HChar_core
	.export		_VDC_VChar_core
	.export		_VDC_CopyMemToVDC_core
	.export		_VDC_CopyVDCToMem_core
	.export		_VDC_FillArea_core
    .export		_VDC_regadd
	.export		_VDC_regval
	.export		_VDC_addrh
	.export		_VDC_addrl
	.export		_VDC_desth
	.export		_VDC_destl
	.export 	_VDC_strideh
	.export		_VDC_stridel
	.export		_VDC_value
	.export		_VDC_tmp1
	.export		_VDC_tmp2
	.export		_VDC_tmp3
	.export		_VDC_tmp4

VDC_ADDRESS_REGISTER    = $D600
VDC_DATA_REGISTER       = $D601

.segment	"CODE"

_VDC_regadd:
	.res	1
_VDC_regval:
	.res	1
_VDC_addrh:
	.res	1
_VDC_addrl:
	.res	1
_VDC_desth:
	.res	1
_VDC_destl:
	.res	1
_VDC_strideh:
	.res	1
_VDC_stridel:
	.res	1
_VDC_value:
	.res	1
_VDC_tmp1:
	.res	1
_VDC_tmp2:
	.res	1
_VDC_tmp3:
	.res	1
_VDC_tmp4:
	.res	1
ZPtmp1:
	.res	1
ZPtmp2:
	.res	1
ZPtmp3:
	.res	1
ZPtmp4:
	.res	1

; Generic helper routines

; ------------------------------------------------------------------------------------------
VDC_Read:
; Function to do a VDC read and wait for ready status
; Input:	X = register number
; Output:	A = read value
; ------------------------------------------------------------------------------------------

	stx VDC_ADDRESS_REGISTER            ; Store X in VDC address register
notyetreadyread:						; Start of wait loop to wait for VDC status ready
	bit VDC_ADDRESS_REGISTER            ; Check status bit 7 of VDC address register
	bpl notyetreadyread                 ; Continue loop if status is not ready
	lda VDC_DATA_REGISTER               ; Load data to A from VDC data register
	rts

; ------------------------------------------------------------------------------------------
VDC_Write:
; Function to do a VDC read and wait for ready status
; Input:	X = register number
; 			A = value to write
; ------------------------------------------------------------------------------------------

	stx VDC_ADDRESS_REGISTER            ; Store X in VDC address register
notyetreadywrite:						; Start of wait loop to wait for VDC status ready
	bit VDC_ADDRESS_REGISTER            ; Check status bit 7 of VDC address register
	bpl notyetreadywrite                ; Continue loop if status is not ready
	sta VDC_DATA_REGISTER               ; Store A to VDC data
	rts

; ------------------------------------------------------------------------------------------
SaveZP:
; Function to safeguard memory configuration and ZP addresses and set selected MMU
; Output:	ZPtmp1		= Temporary location to safeguard ZP $fb
;			ZPtmp2		= Temporary location to safeguard ZP $fc
; ------------------------------------------------------------------------------------------

	; Store $FA and $FB addresses for safety to be restored at exit
	lda $fb								; Obtain present value at $fb
	sta ZPtmp1							; Store to be restored later
	lda $fc								; Obtain present value at $fc
	sta ZPtmp2							; Store to be restored later
	rts

; ------------------------------------------------------------------------------------------
RestoreZP:
; Function to restore memory configuration and ZP addresses and set selected MMU
; Input:	ZPtmp1		= Temporary location to safeguard ZP $fb
;			ZPtmp2		= Temporary location to safeguard ZP $fc
; ------------------------------------------------------------------------------------------

	; Restore $fb and $fc
	lda ZPtmp1							; Obtain stored value of $fb
	sta $fb								; Restore value
	lda ZPtmp2							; Obtain stored value of $fc
	sta $fc								; Restore value
    rts

; Core routines

; ------------------------------------------------------------------------------------------
_VDC_ReadRegister_core:
; Function to read a VDC register
; Input:	VDC_regadd = register number
; Output:	VDC_regval = read value
; ------------------------------------------------------------------------------------------

	ldx _VDC_regadd                     ; Load register address in X
	jsr VDC_Read						; Read VDC
	sta _VDC_regval                     ; Load A to return variable
    rts

; ------------------------------------------------------------------------------------------
_VDC_WriteRegister_core:
; Function to write a VDC register
; Input:	VDC_regadd = register numnber
;			VDC_regval = value to write
; ------------------------------------------------------------------------------------------

    ldx _VDC_regadd                     ; Load register address in X
	lda _VDC_regval				        ; Load register value in A
	jsr VDC_Write						; Write VDC
    rts

; ------------------------------------------------------------------------------------------
_VDC_Poke_core:
; Function to store a value to a VDC address
; Input:	VDC_addrh = VDC address high byte
;			VDC_addrl = VDC address low byte
;			VDC_value = value to write
; ------------------------------------------------------------------------------------------

    ldx #$12                            ; Load $12 for register 18 (VDC RAM address high) in X	
	lda _VDC_addrh                      ; Load high byte of address in A
	jsr VDC_Write						; Write VDC
	inx		    						; Increase X for register 19 (VDC RAM address low)
	lda _VDC_addrl      				; Load low byte of address in A
	jsr VDC_Write						; Write VDC
	ldx #$1f    						; Load $1f for register 31 (VDC data) in X	
	lda _VDC_value       				; Load value to write in A
	jsr VDC_Write						; Write VDC
    rts

; ------------------------------------------------------------------------------------------
_VDC_Peek_core:
; Function to read a value from a VDC address
; Input:	VDC_addrh = VDC address high byte
;			VDC_addrl = VDC address low byte
; Output:	VDC_value = read value
; ------------------------------------------------------------------------------------------

    ldx #$12    						; Load $12 for register 18 (VDC RAM address high) in X	
	lda _VDC_addrh      				; Load high byte of address in A
	jsr VDC_Write						; Write VDC
	inx					    			; Increase X for register 19 (VDC RAM address low)
	lda _VDC_addrl	    	    		; Load low byte of address in A
	jsr VDC_Write						; Write VDC
	ldx #$1f    						; Load $1f for register 31 (VDC data) in X	
	jsr VDC_Read						; Read VDC
	sta _VDC_value			        	; Load A to return variable
    rts

; ------------------------------------------------------------------------------------------
_VDC_DetectVDCMemSize_core:
; Function to detect the VDC memory size
; Output:	VDC_value = memory size in KB (16 or 64)
; ------------------------------------------------------------------------------------------

	; Setting memory mode to 64KB
	; Reading register 28, safeguarding value, setting bit 4 and storing back to register 28
	ldx #$1c							; Load $1c for register 28 in X
	jsr VDC_Read						; Read VDC
	tay									; Transfer A to Y to save value for later restore
	ora #$10							; Set bit 4 of A
	jsr VDC_Write						; Write VDC

	; Writing a $00 value to VDC $1fff
	ldx #$12                            ; Load $12 for register 18 (VDC RAM address high) in X	
	lda #$1f                    		; Load high byte of address in A
	jsr VDC_Write						; Write VDC
	inx		    						; Increase X for register 19 (VDC RAM address low)
	lda #$ff      						; Load low byte of address in A
	jsr VDC_Write						; Write VDC
	ldx #$1f    						; Load $1f for register 31 (VDC data) in X	
	lda #$00       						; Load value to store in A
	jsr VDC_Write						; Write VDC

	; Writing a $ff value to VDC $9fff
	ldx #$12                            ; Load $12 for register 18 (VDC RAM address high) in X	
	lda #$9f                    		; Load high byte of address in A
	jsr VDC_Write						; Write VDC
	inx		    						; Increase X for register 19 (VDC RAM address low)
	lda #$ff      						; Load low byte of address in A
	jsr VDC_Write						; Write VDC
	ldx #$1f    						; Load $1f for register 31 (VDC data) in X	
	lda #$ff       						; Load value to store in A
	jsr VDC_Write						; Write VDC

	; Reading back value of VDC $1fff
    ldx #$12    						; Load $12 for register 18 (VDC RAM address high) in X	
	lda #$1f		     				; Load high byte of address in A
	jsr VDC_Write						; Write VDC
	inx					    			; Increase X for register 19 (VDC RAM address low)
	lda #$ff    	    				; Load low byte of address in A
	jsr VDC_Write						; Write VDC
	ldx #$1f    						; Load $1f for register 31 (VDC data) in X	
	jsr VDC_Read						; Read VDC

	; Comparing value with $ff to see if 64KB address could be read
	bne sixteendetected					; If not equal 16KB detected, so branch
	lda #64								; Load 64 as value to A
	jmp dmsend							; Jump to end of routine
sixteendetected:						; Label for 16KB detected
	lda #16								; Load 16 as value to A

	; Restore bit 4 of register 28
dmsend:									; Label for end of routine
	sta _VDC_value						; Load KB size to return value
	tya									; Transfer value stored in Y back to A
	ldx #$1c							; Store $1c in A for register 28
	jsr VDC_Write						; Write VDC
	rts									; Return

; ------------------------------------------------------------------------------------------
vdc_disable_display:
; Function to disable VDC display
; ------------------------------------------------------------------------------------------
        lda #$80
        ldx #$22
        jsr VDC_Write
        rts

; ------------------------------------------------------------------------------------------
vdc_enable_display:
; ------------------------------------------------------------------------------------------
        lda #$7d
        ldx #$22
        jsr VDC_Write
        rts

; ------------------------------------------------------------------------------------------
WipeVDCMem:
; Function to wipe VDC memory to avoid visible screen corruption on VDC mem lauout change
; ------------------------------------------------------------------------------------------

	; Set start variables
	ldy #$FF							; Set page counter at 255
	lda #$00							; Set value for start address low and high
	sta _VDC_addrh						; Store high byte
	sta _VDC_addrl						; Store low byte

loopWipePage:
	; Hi-byte of the destination address to register 18
	ldx #$12    						; Load $12 for register 18 (VDC RAM address high) in X	
	lda _VDC_addrh	        			; Load high byte of start in A
	jsr VDC_Write						; Write VDC

	; Lo-byte of the destination address to register 19
	ldx #$13    						; Load $13 for register 19 (VDC RAM address high) in X	
	lda _VDC_addrl		        		; Load high byte of start in A
	jsr VDC_Write						; Write VDC

	; Store character to write in data register 31
	ldx #$1f    						; Load $1f for register 31 (VDC data) in X	
	lda #$00			        		; Load zero to wipe
	jsr VDC_Write						; Write VDC

	; Clear the copy bit (bit 7) of register 24 (block copy mode)
	ldx #$18    						; Load $18 for register 24 (block copy mode) in X	
	lda #$00				        	; Load 0 in A
	jsr VDC_Write						; Write VDC

	; Store lenth in data register 30
	ldx #$1e    						; Load $1f for register 30 (word count) in X	
	lda #$ff				        	; Load 255 in A for full page
	jsr VDC_Write						; Write VDC

	; Increase start address with 80 for next line
	inc _VDC_addrh						; Increase high byte

	; Decrease line counter and loop until zero
	dey									; Decrease page counter
	bne loopWipePage					; Continue until counter is zero
	rts

; ------------------------------------------------------------------------------------------
_VDC_SetExtendedVDCMemSize:
; Function to set VDC in 64k memory configuration
; NB: Charsets need to be copied from ROM again after doing this
; ------------------------------------------------------------------------------------------

	jsr vdc_disable_display				; Disable VDC display
	jsr WipeVDCMem						; Wiping memory

	; Setting memory mode to 64KB
	; Reading register 28, safeguarding value, setting bit 4 and storing back to register 28
	ldx #$1c							; Load $1c for register 28 in X
	jsr VDC_Read						; Read VDCregister
	ora #$10							; Set bit 4 of A
	jsr VDC_Write						; Write VDC

	; Kernal call to DLCHR kernal function to copy charsets from ROM to VDC
	jsr	$FF62							; initialize 8563 char. defns.
	
	jsr	vdc_enable_display				; Enable VDC display
	rts

; ------------------------------------------------------------------------------------------
_VDC_DefaultVDCMemSize:
; Function to set VDC in default memory configuration
; NB: Charsets need to be copied from ROM again after doing this
; ------------------------------------------------------------------------------------------

	jsr vdc_disable_display				; Disable VDC display
	jsr WipeVDCMem						; Wiping memory

	; Setting memory mode to 64KB
	; Reading register 28, safeguarding value, setting bit 4 and storing back to register 28
	ldx #$1c							; Load $1c for register 28 in X
	jsr VDC_Read						; Read VDCregister
	and #$EF							; Clear bit 4 of A
	jsr VDC_Write						; Write VDC

	; Kernal call to DLCHR kernal function to copy charsets from ROM to VDC
	jsr	$FF62							; initialize 8563 char. defns.

	jsr	vdc_enable_display				; Enable VDC display
	rts

; ------------------------------------------------------------------------------------------
_VDC_MemCopy_core:
; Function to copy memory from one to another position within VDC memory
; Input:	VDC_addrh = high byte of source address
;			VDC_addrl = low byte of source address
;			VDC_desth = high byte of destination address
;			VDC_destl = low byte of destination address
;			VDC_tmp1 = number of 256 byte pages to copy
;			VDC_tmp2 = length in last page to copy
; ------------------------------------------------------------------------------------------

loopmemcpy:
	; Hi-byte of the destination address to register 18
	ldx #$12    						; Load $12 for register 18 (VDC RAM address high) in X	
	lda _VDC_desth      				; Load high byte of dest in A
	jsr VDC_Write						; Write VDC

	; Lo-byte of the destination address to register 19
	ldx #$13    						; Load $13 for register 19 (VDC RAM address high) in X	
	lda _VDC_destl       				; Load high byte of dest in A
	jsr VDC_Write						; Write VDC

	; Set the copy bit (bit 7) of register 24 (block copy mode)
	ldx #$18    						; Load $18 for register 24 (block copy mode) in X	
	lda #$80			        		; Set copy bit
	jsr VDC_Write						; Write VDC

	; Hi-byte of the source address to block copy source register 32
	ldx #$20					    	; Load $20 for register 32 (block copy source) in X	
	lda _VDC_addrh			        	; Load high byte of source in A
	jsr VDC_Write						; Write VDC
	
	; Lo-byte of the source address to block copy source register 33
	ldx #$21					    	; Load $21 for register 33 (block copy source) in X	
	lda _VDC_addrl		        		; Load low byte of source in A
	jsr VDC_Write						; Write VDC
	
	; Number of bytes to copy
	ldx #$1E    						; Load $1E for register 30 (word count) in X
	lda _VDC_tmp1		        		; Load page counter in A
	cmp #$01    						; Check if this is the last page
	bne notyetlastpage			        ; Branch to 'not yet last page' if not equal
	lda _VDC_tmp2		        		; Set length in last page
	jmp lastpage		        		; Goto last page label
notyetlastpage:							; Label for not yet last page
	lda #$ff    						; Set length for 256 bytes
lastpage:								; Label for jmp if last page
	jsr VDC_Write						; Write VDC

	; Decrease page counter and loop until last page
	inc _VDC_desth		        		; Increase destination address page counter
	inc _VDC_addrh		        		; Increase source address page counter
	dec _VDC_tmp1		        		; Decrease page counter
	bne loopmemcpy				        ; Repeat loop until page counter is zero
    rts

; ------------------------------------------------------------------------------------------
_VDC_HChar_core:
; Function to draw horizontal line with given character (draws from left to right)
; Input:	VDC_addrh = igh byte of start address
;			VDC_addrl = ow byte of start address
;			VDC_tmp1 = character value
;			VDC_tmp2 = length value
;			VDC_tmp3 = attribute value
; ------------------------------------------------------------------------------------------

	; Hi-byte of the destination address to register 18
	ldx #$12    						; Load $12 for register 18 (VDC RAM address high) in X	
	lda _VDC_addrh	        			; Load high byte of start in A
	jsr VDC_Write						; Write VDC

	; Lo-byte of the destination address to register 19
	ldx #$13    						; Load $13 for register 19 (VDC RAM address high) in X	
	lda _VDC_addrl		        		; Load high byte of start in A
	jsr VDC_Write						; Write VDC

	; Store character to write in data register 31
	ldx #$1f    						; Load $1f for register 31 (VDC data) in X	
	lda _VDC_tmp1			        	; Load character value in A
	jsr VDC_Write						; Write VDC

	; Clear the copy bit (bit 7) of register 24 (block copy mode)
	ldx #$18    						; Load $18 for register 24 (block copy mode) in X	
	lda #$00				        	; Load 0 in A
	jsr VDC_Write						; Write VDC

	; Store lenth in data register 30
	ldx #$1e    						; Load $1f for register 30 (word count) in X	
	lda _VDC_tmp2			        	; Load character value in A
	jsr VDC_Write						; Write VDC

	; Continue with copying attribute values
	clc									; Clear carry
	lda _VDC_addrh						; Load high byte of start address again in A
	adc #$08							; Add 8 pages to get charachter attribute address

	; Hi-byte of the destination attribute address to register 18
	ldx #$12    						; Load $12 for register 18 (VDC RAM address high) in X	
	jsr VDC_Write						; Write VDC

	; Lo-byte of the destination attribute address to register 19
	ldx #$13    						; Load $13 for register 19 (VDC RAM address high) in X	
	lda _VDC_addrl		        		; Load high byte of start in A
	jsr VDC_Write						; Write VDC

	; Store attribute to write in data register 31
	ldx #$1f    						; Load $1f for register 31 (VDC data) in X	
	lda _VDC_tmp3			        	; Load attribute value in A
	jsr VDC_Write						; Write VDC

	; Clear the copy bit (bit 7) of register 24 (block copy mode)
	ldx #$18    						; Load $18 for register 24 (block copy mode) in X	
	lda #$00				        	; Load prepared value with bit 7 set in A
	jsr VDC_Write						; Write VDC

	; Store lenth in data register 30
	ldx #$1e    						; Load $1f for register 30 (word count) in X	
	lda _VDC_tmp2			        	; Load character value in A
	jsr VDC_Write						; Write VDC
    rts

; ------------------------------------------------------------------------------------------
_VDC_VChar_core:
; Function to draw vertical line with given character (draws from top to bottom)
; Input:	VDC_addrh = high byte of start address
;			VDC_addrl = low byte of start address
;			VDC_tmp1 = character value
;			VDC_tmp2 = length value
;			VDC_tmp3 = attribute value
; ------------------------------------------------------------------------------------------

loopvchar:
	; Hi-byte of the destination address to register 18
	ldx #$12    						; Load $12 for register 18 (VDC RAM address high) in X	
	lda _VDC_addrh		        		; Load high byte of start in A
	jsr VDC_Write						; Write VDC

	; Lo-byte of the destination address to register 19
	ldx #$13    						; Load $13 for register 19 (VDC RAM address high) in X	
	lda _VDC_addrl			        	; Load high byte of start in A
	jsr VDC_Write						; Write VDC

	; Store character to write in data register 31
	ldx #$1f    						; Load $1f for register 31 (VDC data) in X	
	lda _VDC_tmp1			        	; Load character value in A
	jsr VDC_Write						; Write VDC

	; Continue with attribute value
	clc									; CLear carry
	lda _VDC_addrh						; Load high byte of start address again in A
	adc #$08							; Add 8 pages to get charachter attribute address

	; Hi-byte of the destination attribute address to register 18
	ldx #$12    						; Load $12 for register 18 (VDC RAM address high) in X	
	jsr VDC_Write						; Write VDC

	; Lo-byte of the destination attribute address to register 19
	ldx #$13    						; Load $13 for register 19 (VDC RAM address high) in X	
	lda _VDC_addrl			        	; Load high byte of start in A
	jsr VDC_Write						; Write VDC

	; Store attribute to write in data register 31
	ldx #$1f    						; Load $1f for register 31 (VDC data) in X	
	lda _VDC_tmp3			        	; Load attribute value in A
	jsr VDC_Write						; Write VDC

	; Increase start address with 80 for next line
	clc 								; Clear carry
	lda _VDC_addrl	        			; Load low byte of address to A
	adc #$50    						; Add 80 with carry
	sta _VDC_addrl			        	; Store result back
	lda _VDC_addrh	        			; Load high byte of address to A
	adc #$00    						; Add 0 with carry
	sta _VDC_addrh	        			; Store result back

	; Loop until length reaches zero
	dec _VDC_tmp2		        		; Decrease length counter
	bne loopvchar		        		; Loop if not zero
    rts

; ------------------------------------------------------------------------------------------
_VDC_CopyMemToVDC_core:
; Function to copy memory from VDC memory to standard memory
; Input:	VDC_addrh = high byte of source address
;			VDC_addrl = low byte of source address
;			VDC_desth = high byte of VDC destination address
;			VDC_destl = low byte of VDC destination address
;			VDC_tmp1 = number of 256 byte pages to copy
;			VDC_tmp2 = length in last page to copy
; ------------------------------------------------------------------------------------------

	jsr SaveZP							; Safeguard ZP

	; Set address pointer in zero-page
	lda _VDC_addrl						; Obtain low byte in A
	sta $fb								; Store low byte in pointer
	lda _VDC_addrh						; Obtain high byte in A
	sta $fc								; Store high byte in pointer

	; Hi-byte of the source VDC address to register 18
	ldx #$12    						; Load $12 for register 18 (VDC RAM address high) in X	
	lda _VDC_desth		        		; Load high byte of address in A
	jsr VDC_Write						; Write VDC

	; Low-byte of the source VDC address to register 19
	inx 								; Increase X for register 19 (VDC RAM address low)
	lda _VDC_destl      				; Load low byte of address in A
	jsr VDC_Write						; Write VDC

	; Start of copy loop
	ldy #$00    						; Set Y as counter on 0
	
	; Read value and store at VDC address
copyloopm2v:							; Start of copy loop
	lda ($fb),y							; Load source data
	ldx #$1f    						; Load $1f for register 31 (VDC data) in X
	jsr VDC_Write						; Write VDC

	; Increase source address (VDC auto increments)
	inc $fb								; Increment low byte of source address
	bne nextm2v1						; If not yet zero, branch to next label
	inc $fc								; Increment high byte of source address
nextm2v1:								; Next label
	dec _VDC_tmp2						; Decrease low byte of length
	lda _VDC_tmp2						; Load low byte of length to A
	cmp #$ff							; Check if below zero
	bne copyloopm2v						; Continue loop if not yet below zero
	dec _VDC_tmp1						; Decrease high byte of length
	lda _VDC_tmp1						; Load high byte of length to A
	cmp #$ff							; Check if below zero
	bne copyloopm2v						; Continue loop if not yet below zero

	jsr RestoreZP						; Restore ZP
	rts

; ------------------------------------------------------------------------------------------
_VDC_CopyVDCToMem_core:
; Function to copy memory from VDC memory to standard memory
; Input:	VDC_addrh = high byte of VDC source address
;			VDC_addrl = low byte of VDC source address
;			VDC_desth = high byte of destination address
;			VDC_destl = low byte of destination address
;			VDC_tmp1 = number of 256 byte pages to copy
;			VDC_tmp2 = length in last page to copy
; ------------------------------------------------------------------------------------------

	jsr SaveZP							; Safeguard ZP

	; Set address pointer in zero-page and STAVEC vector
	lda _VDC_destl						; Obtain low byte in A
	sta $fb								; Store low byte in pointer
	lda _VDC_desth						; Obtain high byte in A
	sta $fc								; Store high byte in pointer
	lda #$fb							; Load $fb address value in A
	sta $2b9							; Save in STAVEC vector

	; Start of copy loop
	ldy #$00    						; Set Y as counter on 0

copyloopv2m:							; Start of copy loop

	; Hi-byte of the source VDC address to register 18
	ldx #$12    						; Load $12 for register 18 (VDC RAM address high) in X	
	lda _VDC_addrh		        		; Load high byte of address in A
	jsr VDC_Write						; Write VDC

	; Low-byte of the source VDC address to register 19
	inx 								; Increase X for register 19 (VDC RAM address low)
	lda _VDC_addrl      				; Load low byte of address in A
	jsr VDC_Write						; Write VDC
	
	; Read VDC value and store at destination address
	ldx #$1f    						; Load $1f for register 31 (VDC data) in X
	jsr VDC_Read						; Read VDC
	sta ($fb),y							; Store in target memory

	; Increase VDC source address and target memory address
	inc $fb								; Increment low byte of target address
	bne nextv2m1						; If not yet zero, branch to next label
	inc $fc								; Increment high byte of target address
nextv2m1:								; Next label
	inc _VDC_addrl						; Increment low byte of VDC address
	bne nextv2m2						; If not yet zero, branch to next label
	inc _VDC_addrh						; Increment hight byte of VDC address
nextv2m2:								; Next label
	dec _VDC_tmp2						; Decrease low byte of length
	lda _VDC_tmp2						; Load low byte of length to A
	cmp #$ff							; Check if below zero
	bne copyloopv2m						; Continue loop if not yet below zero
	dec _VDC_tmp1						; Decrease high byte of length
	lda _VDC_tmp1						; Load high byte of length to A
	cmp #$ff							; Check if below zero
	bne copyloopv2m						; Continue loop if not yet below zero

	jsr RestoreZP						; Restore MU/ZP
    rts

; ------------------------------------------------------------------------------------------
_VDC_FillArea_core:
; Function to draw area with given character (draws from topleft to bottomright)
; Input:	VDC_addrh = high byte of start address
;			VDC_addrl = low byte of start address
;			VDC_tmp1 = haracter value
;			VDC_tmp2 = length value
;			VDC_tmp3 = attribute value
;			VDC_tmp4 = number of lines
; ------------------------------------------------------------------------------------------

loopdrawline:
	jsr _VDC_HChar_core					; Draw line

	; Increase start address with 80 for next line
	clc 								; Clear carry
	lda _VDC_addrl	        			; Load low byte of address to A
	adc #$50    						; Add 80 with carry
	sta _VDC_addrl			        	; Store result back
	lda _VDC_addrh	        			; Load high byte of address to A
	adc #$00    						; Add 0 with carry
	sta _VDC_addrh	        			; Store result back

	; Decrease line counter and loop until zero
	dec _VDC_tmp4						; Decrease line counter
	bne loopdrawline					; Continue until counter is zero
	rts