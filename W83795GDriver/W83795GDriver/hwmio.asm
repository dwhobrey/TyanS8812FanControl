; hwmio.asm, (C) 2013 Tyan Corp & D.Whobrey.
; S8812 / W83795G access code.
; ml64, amd64.

include ksamd64.inc

smbusio macro Reg
        mov    dx,di
        add    dx,Reg    
endm

preamble macro
        rex_push_reg rbp
        alloc_stack 40h
        lea    rbp,[rsp+020h]
        .setframe rbp,020h
        END_PROLOGUE
endm

postamble macro
        lea    rsp,[rbp-020h]
        add    rsp,40h
        pop    rbp
        ret_zero
endm

.code

; ShortDelay.
LEAF_ENTRY ShortDelay, _TEXT 
		push   rax
		push   rcx  
		mov    cx,14h
 loop1: in     ax,80h
        jmp    @F
    @@: jmp    @F
    @@: loop   loop1
        pop    rcx
        pop    rax   
        ret
LEAF_END ShortDelay, _TEXT
    
; LongDelay.
LEAF_ENTRY LongDelay, _TEXT
        push   rax
        push   rcx   
        mov    cx,64h
 loop1: in     ax,80h
        jmp    @F
    @@: jmp    @F
    @@: loop   loop1
        pop    rcx
        pop    rax 
        ret 
LEAF_END LongDelay, _TEXT 

; Enable SmbusSel2, IMB GPIO 11/12 as SmBus Clk/Data.
LEAF_ENTRY SelectSmbus, _TEXT
		push   rax
		push   rdx
		mov    eax,8000a0d0h  ; I2CBusConfig reg, dev 14h fn 0, reg d0/d2.
		mov    dx,0cf8h
		out    dx,eax
		mov    dx,0cfeh  ; Access d2h.
		in     al,dx
		or     al,40h    ; Set bit 6: SmbusSel2.
		out    dx,al
		pop    rdx
		pop    rax
		ret
LEAF_END SelectSmbus, _TEXT
   
; Disable SmbusSel2.
LEAF_ENTRY DeselectSmbus, _TEXT  
		push   rax
		push   rdx
		mov    eax,8000a0d0h ; I2CBusConfig reg, dev 14h fn 0, reg d0/d2.
		mov    dx,0cf8h
		out    dx,eax
		mov    dx,0cfeh  ; Access d2h.
		in     al,dx
		and    al,0bfh   ; Clear bit 6: SmbusSel2.
		out    dx,al
		pop    rdx
		pop    rax
		ret 
LEAF_END DeselectSmbus, _TEXT 

; Init Slave & wait for smbus transaction to complete, 
; kill if necessary. 
LEAF_ENTRY InitSmbus, _TEXT
        push   rax
        push   rdx
        push   rbx
  smbs: mov    dx,0b00h   ;  smbus status.
        mov    bl,80h
 loop1: in     al,dx
        mov    ah,0ffh
    @@: out    0ebh,al
        dec    ah
        jne    @B
        test   al,1h      ;  check busy bit.
        je     slvs
        dec    bl
        jne    loop1
        mov    dx,0b02h   ; smbus control.
        in     al,dx
        or     al,2h      ; set kill bit.
        out    dx,al
        jmp    smbs
  slvs: mov    dx,0b01h   ; slave status.
        mov    bl,80h
 loop3: in     al,dx
        mov    ah,0ffh
 loop2: out    0ebh,al
        dec    ah
        jne    loop2
        test   al,1h      ; check if slave busy.
        je     exit1
        dec    bl
        jne    loop3
        or     al,2h      ; init slave.
        out    dx,al
        jmp    slvs
 exit1: pop    rbx
        pop    rdx
        pop    rax
        ret 
LEAF_END InitSmbus, _TEXT 
  
; SMBus Command & Byte Read/Write io protocol.
; ah=Slave adrs (already < 1 + r/w bit).
; al=Command byte.
; cl=Byte to read / write for command.
; On exit:
; if no errors: al/cl=byte read / write, ah=Slave adrs, CF=clear.
; if errors: al=status flags, ah=0, cl=0, CF=set.
LEAF_ENTRY ReadWriteSmbus, _TEXT 
        push   rdx
        push   rdi
        pushf  
        cli    
        push   rax
        mov    eax,8000a090h ; PCI reg for smbus base adrs.
        mov    dx,0cf8h
        out    dx,eax
        mov    dx,0cfch
        in     ax,dx
        and    ax,0fffeh
        mov    di,ax       ; Save smbus base adrs in di.
        mov    dx,di
        add    dx,0h       ; Status reg.
    @@: mov    al,1eh      ; Mask for clearing error bits.
        out    dx,al       ; Clear status flags.
        in     al,dx
        test   al,1h
        jne    @B          ; jnz loop.
        pop    rax
        mov    dx,di
        add    dx,4h       ; Slave adrs reg.
        xchg   al,ah
        out    dx,al       ; set slave adrs.
        mov    dx,di
        add    dx,3h       ; Cmd reg. 
        xchg   al,ah
        out    dx,al       ; set cmd value.
        test   ah,1h       ; Check if read or write.
        jne    @F          ; jnz read
        mov    dx,di
        add    dx,5h       ; Data0 reg.
        mov    al,cl       ; set output data byte.
        out    dx,al
    @@: mov    dx,di
        add    dx,2h       ; Control reg.
        mov    al,48h      ; Init & set protocol to Byte Data r/w.
        out    dx,al
        call   LongDelay   ; Delay.
        mov    dx,di
        add    dx,0h       ; Status reg.
    @@: call   ShortDelay  ; Delay.
        in     al,dx
        test   al,1h
        jne    @B          ; jnz loop until complete.
        test   al,1ch      ; Check for error.
        jne    exit1       ; jnz exit error.
        test   ah,1h       ; Check if r/w.
        je     exit3       ; jz it was a write.
        mov    dx,di
        add    dx,5h       ; Data0 reg.
        in     al,dx       ; Read data.
        mov    cl,al
 exit3: popf 
        clc
        jmp    exit2
 exit1: popf               ; exit error, al=status flags, ah=0, cl=0.
        mov    ah,0h       
        mov    cl,0h
        stc
 exit2: pop    rdi
        pop    rdx
        ret   
LEAF_END ReadWriteSmbus, _TEXT

LEAF_ENTRY ReadSmbus, _TEXT
		or     ah,1h    ; Set read bit.
		call   InitSmbus
		call   ReadWriteSmbus
		ret 
LEAF_END ReadSmbus, _TEXT

LEAF_ENTRY WriteSmbus, _TEXT
		and    ah,0feh  ; Clear read bit.
		call   InitSmbus 
		call   ReadWriteSmbus
		ret 
LEAF_END WriteSmbus, _TEXT

; Read register value using currently selected bank.
; cl = slvAdrs, dl = reg.
; Returns: 
; if no errors: al = val, ah=0.
; if errors: al=0, ah=status.
NESTED_ENTRY ReadRegister, _TEXT
		preamble
		call   SelectSmbus
		mov    ah,cl
		mov    al,dl	
		call   ReadSmbus
		jc     @F
		mov    ah,0h
		jmp    exit1
    @@: xchg   ah,al
 exit1:	call   DeselectSmbus
		postamble
NESTED_END ReadRegister, _TEXT 

; Write to register using currently selected bank.
; cl = slvAdrs, dl = reg, r8b = val.
; Returns: 
; if no errors: al = val, ah=0.
; if errors: al=0, ah=status.
NESTED_ENTRY WriteRegister, _TEXT
		preamble
		push   rcx
		call   SelectSmbus
		mov    ah,cl
		mov    al,dl
		mov    cl,r8b	
		call   WriteSmbus
		jc     @F
		mov    ah,0h
		jmp    exit1
    @@: xchg   ah,al		
 exit1: call   DeselectSmbus
		pop    rcx
		postamble
NESTED_END WriteRegister, _TEXT 

; Select bank and read from register.
; cl = slvadrs, dl = reg, r8b = bank.
; Returns: 
; if no errors: al = val, ah=0.
; if errors: al=0, ah=status.
NESTED_ENTRY SelectBankAndReadRegister, _TEXT
		preamble
		push   rcx 
		call   SelectSmbus
		; Select bank.
		mov    ah,cl
		mov    al,0h      ; bank select reg.
		mov    cl,r8b 
		call   WriteSmbus
		jc     @F
		pop    rcx
		push   rcx
		mov    ah,cl
		mov    al,dl
		call   ReadSmbus
		jc     @F
		mov    ah,0h
		jmp    exit1
   @@:  xchg   ah,al
exit1:	call   DeselectSmbus
        pop    rcx
		postamble  
NESTED_END SelectBankAndReadRegister, _TEXT

; Select bank and read word from registers.
; cl = slvadrs, dl = reg1, r8b = bank, r9b = reg2.
; Returns:
; First read in high word of eax. Second read in low word. 
; if no errors: al = val, ah=0.
; if errors: al=0, ah=status.
NESTED_ENTRY SelectBankAndReadRegisterWord, _TEXT
		preamble
		push   rcx 
		call   SelectSmbus
		; Select bank.
		mov    ah,cl
		mov    al,0h      ; bank select reg.
		mov    cl,r8b 
		call   WriteSmbus
		jc     @F
		pop    rcx
		push   rcx
		mov    ah,cl
		mov    al,dl
		call   ReadSmbus
		jc     @F
		pop    rcx
		push   rcx
		mov    ah,0h
		rol    eax,16
		mov    ah,cl
		mov    al,r9b
		or     ah,1h    ; Set read bit.
		call   ReadWriteSmbus
		jc     @F
		mov    ah,0h
		jmp    exit1
   @@:  xchg   ah,al
exit1:	call   DeselectSmbus
        pop    rcx
		postamble  
NESTED_END SelectBankAndReadRegisterWord, _TEXT

; Select bank and write to register.
; cl = slvadrs, dl = reg, r8b = bank, r9b = val.
; Returns:
; if no errors: al = val, ah=0.
; if errors: al=0, ah=status.
NESTED_ENTRY SelectBankAndWriteRegister, _TEXT
		preamble
		push   rcx 
		call   SelectSmbus
		; Select bank.
		mov    ah,cl
		mov    al,0h      ; bank select reg.
		mov    cl,r8b 
		call   WriteSmbus
		jc     @F
		pop    rcx
		push   rcx
		mov    ah,cl
		mov    al,dl
		mov    cl,r9b
		call   WriteSmbus
		jc     @F
		mov    ah,0h
		jmp    exit1
   @@:  xchg   ah,al
exit1:	call   DeselectSmbus
        pop    rcx
		postamble   
NESTED_END SelectBankAndWriteRegister, _TEXT

; Get W83795G slv adrs in al.
; Tries in turn: 58,5a,5c,5e.
; Returns: 
; if no errors: al=slv adrs, ah=0.
; if errors: al=0, ah=0.
NESTED_ENTRY GetSlaveAdrs, _TEXT 
		preamble  
        push   rcx
        push   rsi
        call   SelectSmbus
        lea    rsi,SlvAddresses
        jmp    @F
 loop1: add    rsi,1h
    @@: mov    ah,byte ptr [rsi]  ; Slave adrs to try.
        cmp    ah,0h
        je     exit1         ; jz Exit device not found.
        mov    al,0feh       ; Get Chip ID.
        call   ReadSmbus
        cmp    cl,79h        ; ID for W83795G.
        jne    loop1         ; jnz loop, try next adrs.
        mov    ah,byte ptr [rsi]
        mov    al,0h
        mov    cl,0h
        call   WriteSmbus 
        mov    ah,byte ptr [rsi]
        mov    al,0fdh        ; Get Vendor ID.
        call   ReadSmbus
        cmp    cl,0a3h
        jne    loop1
        mov    ah,byte ptr [rsi]
        mov    al,0h          ; Bank select, zero.
        mov    cl,80h         ; Request high byte for V ID.
        call   WriteSmbus 
        mov    ah,byte ptr [rsi]
        mov    al,0fdh        ; Get Vendor ID, high byte
        call   ReadSmbus
        cmp    cl,5ch
        jne    loop1          ; jnz loop.
        mov    ah,0h
        mov    al,byte ptr [rsi]
        jmp    exit2
 exit1: mov    al,0h   
 exit2: call   DeselectSmbus    
        pop    rsi
        pop    rcx
        postamble  
SlvAddresses db 58h, 5ah, 5ch, 5eh, 00h
NESTED_END GetSlaveAdrs, _TEXT

; SB-TSI routines.

; Read SB-TSI command via STCC & STMMCR3 regs.
; Select bank 3, set STMMCR1 to MME & Read,
; set STCC command & read when ready.
; cl = slvadrs, dl = cmd, r8b = DTSx, x=0..7.
; Returns:
; if no errors: al = val, ah=0.
; if errors: al=0, ah=status.
NESTED_ENTRY SBTSIReadCommand, _TEXT
		preamble
		push   rbx
		push   rcx 
		mov    bl,cl      ; Save slvadrs.
		call   SelectSmbus
		; Select bank 3.
		mov    ah,bl
		mov    al,0h      ; bank select reg.
		mov    cl,3 
		call   WriteSmbus
		jc     @F
		; Select DTSx.
		mov    ah,bl
		mov    al,1        ; 1 << r8b
		mov    cl,r8b
		and    cl,07h
		shl    al,cl
		mov    cl,al
		mov    al,02h      ; DTSE enable reg.
		call   WriteSmbus
		jc     @F
		; Select STMMCR1 reg and set MME & write flags.
		mov    ah,bl
		mov    al,0a4h      ; STMMCR1 reg.
		mov    cl,81h       ; MME enable & read bit set. 
		call   WriteSmbus
		jc     @F
		; Set STCC cmd.
		mov    ah,bl
		mov    al,0a5h      ; STMMCR2 reg.
		mov    cl,dl        ; cmd code. 
		call   WriteSmbus
		jc     @F
		; Perform command by setting STOSS OneShot.
		mov    ah,bl
		mov    al,0a3h		; STOSS.
		mov    cl,01h	    ; start OneShot.
		call   WriteSmbus
		jc     @F
		; Loop until OneShot finishes.
retry:	mov    ah,bl
		mov    al,0a3h      ; Loop until STOSS bit 0 is clear.
		call   ReadSmbus
		jc     @F
		and    al,01h
		jnz    retry
		; Read data from STRD
		mov    ah,bl
		mov    al,0a8h		; STRD reg.
		call   ReadSmbus
		jc     @F
		mov    bh,al       ; Save read val in bh.
		; Reenable all DTSx.
		mov    ah,bl
		mov    al,02h      ; DTSE enable reg.
		mov    cl,0fh      ; Enable first 4 on S8812.
		call   WriteSmbus
		jc     @F
		; Retore STMCR1.
		mov    ah,bl
		mov    al,0a4h		; STMMCR1.
		mov    cl,01h	    ; Clear MME & OSE, set read flag.
		call   WriteSmbus
		mov    al,bh        ; Restore read val.
		jc     @F		
		mov    ah,0h
		jmp    exit1
   @@:  xchg   ah,al
exit1:	call   DeselectSmbus
        pop    rcx
		pop    rbx
		postamble   
NESTED_END SBTSIReadCommand, _TEXT

; Write SB-TSI command via STCC & STMMCR3 regs.
; Select bank 3, set STMMCR1 to MME & Write,
; set data & set STCC command.
; cl = slvadrs, dl = cmd, r8b = DTSx, x=0..7, r9b = val.
; Returns:
; if no errors: al = val, ah=0.
; if errors: al=0, ah=status.
NESTED_ENTRY SBTSIWriteCommand, _TEXT
		preamble
		push   rbx
		push   rcx 
		mov    bl,cl      ; Save slvadrs.
		call   SelectSmbus
		; Select bank 3.
		mov    ah,bl
		mov    al,0h      ; bank select reg.
		mov    cl,3 
		call   WriteSmbus
		jc     @F
		; Select DTSx.
		mov    ah,bl
		mov    al,1        ; 1 << r8b
		mov    cl,r8b
		and    cl,07h
		shl    al,cl
		mov    cl,al
		mov    al,02h      ; DTSE enable reg.
		call   WriteSmbus
		jc     @F
		; Select STMMCR1 reg and set MME & write flags.
		mov    ah,bl
		mov    al,0a4h      ; STMMCR1 reg.
		mov    cl,80h       ; MME enable & read bit clear. 
		call   WriteSmbus
		jc     @F
		; Set STCC cmd.
		mov    ah,bl
		mov    al,0a5h      ; STMMCR2 reg.
		mov    cl,dl        ; cmd code. 
		call   WriteSmbus
		jc     @F
		; Write data to STMMCR3
		mov    ah,bl
		mov    al,0a6h		; STMMCR3.
		mov    cl,r9b	    ; val to write.
		call   WriteSmbus
		jc     @F
		; Perform command by setting STOSS OneShot.
		mov    ah,bl
		mov    al,0a3h		; STOSS.
		mov    cl,01h	    ; start OneShot.
		call   WriteSmbus
		jc     @F
		; Loop until OneShot finishes.
retry:	mov    ah,bl
		mov    al,0a3h      ; Loop until STOSS bit 0 is clear.
		call   ReadSmbus
		jc     @F
		and    al,01h
		jnz    retry
		; Reenable all DTSx.
		mov    ah,bl
		mov    al,02h      ; DTSE enable reg.
		mov    cl,0fh      ; Enable first 4 on S8812.
		call   WriteSmbus
		jc     @F
		; Retore STMCR1
		mov    ah,bl
		mov    al,0a4h		; STMMCR1.
		mov    cl,01h	    ; Clear MME & OSE, set read flag.
		call   WriteSmbus
		jc     @F		
		mov    ah,0h
		jmp    exit1
   @@:  xchg   ah,al
exit1:	call   DeselectSmbus
        pop    rcx
		pop    rbx
		postamble   
NESTED_END SBTSIWriteCommand, _TEXT

END
