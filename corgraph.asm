TITLE   Corona graphics module
;       Colin Kelley
;       November 9, 1986

xmax    equ 640                 ; Corona Screen
ymax    equ 325                 ;


X       equ 6


public  _GrInit,_GrReset,_GrAddr,_GrOnly,_TxOnly,_GrandTx,_Cor_line

_prog   segment para 'code'
        assume cs:_prog
X       equ 6

_Cor_line proc far
        push bp
        mov bp,sp
        push si
        push di
        mov ax,[bp+X]           ; x1
        mov bx,[bp+X+2]         ; y1
        mov cx,[bp+X+4]         ; x2
        mov si,[bp+X+6]         ; y2

        cmp ax,cx               ; x1,x2
        jne i19
        cmp bx,si               ; y1,y2
        jne i19

        call near ptr corpixel

        jmp short i28
i19:
        mov dx,ax               ; dx,x1
        sub dx,cx               ; x2
        jnc noabsx
        neg dx
noabsx:
        mov di,bx               ; dy,y1
        sub di,si               ; y2
        jnc noabsy
        neg di                  ; dy
noabsy:
        cmp dx,di               ; dx,dy
        ja jcc91
        jmp short i21
jcc91:
        cmp ax,cx               ; x1,x2
        jbe i22
        xchg ax,cx              ; x1,x2
        xchg bx,si              ; y1,y2
i22:
        cmp bx,si               ; y1,y2
        jae l20004
        mov si,1                ; y2,1
        jmp short l20005
l20004:
        mov si,-1               ; y2,-1
l20005:
        mov bp,dx               ; sum,dx
        shr bp,1                ; sum,1
d23:
        ;check out mask
        call near ptr corpixel
        add bp,di               ; sum,dy
        cmp bp,dx
        jb i27
        sub bp,dx               ; sum,dx
        add bx,si               ; y1,y2
i27:
        inc ax                  ; x1
        cmp ax,cx               ; x1,x2
        jbe d23
        jmp short i28

; else iterate y's
i21:
        cmp bx,si               ; y1,y2
        jbe i29
        xchg ax,cx              ; x1,x2
        xchg bx,si              ; y1,y2
i29:
        cmp ax,cx               ; x1,x2
        jae l20006
        mov cx,1                ; x2,1
        jmp short l20007
l20006:
        mov cx,-1               ; x2,-1
l20007:
        mov bp,di               ; sum,dy
        shr bp,1                ; sum,1
d30:
        ; check out mask
        call near ptr corpixel
        add bp,dx               ; sum,dx
        cmp bp,di               ; sum,dy
        jb i34
        sub bp,di               ; sum,dy
        add ax,cx               ; x1,x2
i34:
        inc bx                  ; y1
        cmp bx,si               ; y2
        jbe d30
i28:
        pop di
        pop si
        pop bp
        ret
_Cor_line endp


_GrInit proc far
        push bp
        mov bp,sp
        push di
        mov ax, [bp+X]          ; offset of screen
        add ax,15
        mov cl,4
        shr ax,cl
        add ax, [bp+X+2]        ; add segment
        add ax, (32768/16)-1
        and ax, 65535-((32768/16)-1)    ; round up to 32K boundary
        mov cs:ScSeg,ax         ; save segment for later
        push ax
        mov es, ax
        xor ax,ax
        mov di,ax
        mov cx, 4000h
        cld
        rep stosw
        pop cx
        call far ptr _GrAddr
        mov ax,es
        pop di
        pop bp
        ret
_GrInit endp

_GrReset proc far
        mov cx, 0
        call far ptr _GrAddr
        ret
_GrReset endp

_GrAddr proc far
        mov dx,3b4h                     ; address of 6845
        mov al,0ch                      ; register 12
        out dx,al
        inc dx
        mov al,ch                       ; Graphics Segment High
        out dx,al
        dec dx
        mov al,0dh                      ; register 13
        out dx,al
        mov al,cl                       ; Graphics Segment Low
        inc dx
        out dx,al
        ret
_GrAddr endp

_GrOnly proc far
        mov dx,3b8h
        mov al,0a0h
        out dx,al
        ret
_GrOnly endp

_TxOnly proc far
        mov dx,3b8h
        mov al,28h
        out dx,al
        ret
_TxOnly endp

_GrandTx proc far
        mov dx,3b8h
        mov al,0a8h
        out dx,al
        ret
_GrandTx endp

corpixel proc near
        push bp
        mov bp,sp
        push ax
        push bx
        push cx
        mov es,cs:ScSeg
        shl bx,1                        ; y
        mov bx,word ptr cs:LookUp[bx] ; bx has y mem address
        mov cl,al                       ; x
        and cl,7
        shr ax,1
        shr ax,1
        shr ax,1                        ; ax /= 8
        add bx,ax
        mov al,1
        shl al,cl                       ; al contains bit mask
        or byte ptr es:[bx],al
        pop cx
        pop bx
        pop ax
        pop bp
        ret

K       equ 1024

mem_mac MACRO x
        dw x,2*K+x,4*K+x,6*K+x,8*K+x,10*K+x,12*K+x,14*K+x,16*K+x
        dw 18*K+x,20*K+x,22*K+x,24*K+x
        ENDM

ScSeg   dw 0

LookUp  equ $
        mem_mac 0
        mem_mac 80
        mem_mac (80*2)
        mem_mac (80*3)
        mem_mac (80*4)
        mem_mac (80*5)
        mem_mac (80*6)
        mem_mac (80*7)
        mem_mac (80*8)
        mem_mac (80*9)
        mem_mac (80*10)
        mem_mac (80*11)
        mem_mac (80*12)
        mem_mac (80*13)
        mem_mac (80*14)
        mem_mac (80*15)
        mem_mac (80*16)
        mem_mac (80*17)
        mem_mac (80*18)
        mem_mac (80*19)
        mem_mac (80*20)
        mem_mac (80*21)
        mem_mac (80*22)
        mem_mac (80*23)
        mem_mac (80*24)
corpixel endp

_prog   ends

        end
................ ... ...-....1200 N81N         ......................... ... ...-....1200 N81N
