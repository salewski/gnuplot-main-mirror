TITLE   PC graphics module

;       Colin Kelley
;       November 9, 1986

        public _PC_line, _PC_color, _PC_mask, _PC_curloc, _PC_puts, _Vmode

_prog   segment para 'code'
        assume cs:_prog

X       equ 6

_PC_line proc far
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

        call near ptr pcpixel

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
        call near ptr pcpixel
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
        call near ptr pcpixel
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
_PC_line endp

_PC_color proc far
        push bp
        mov bp,sp
        mov al,[bp+X]                   ; color
        mov byte ptr cs:color,al
        pop bp
        ret
_PC_color endp

_PC_mask proc far
        push bp
        mov bp,sp
        mov ax,[bp+X]                   ; mask
        mov word ptr cs:mask,ax
        pop bp
        ret
_PC_mask endp

mask    dw -1
color   db 1

_Vmode  proc far
        push bp
        mov bp,sp
        push si
        push di
        mov ax,[bp+X]
        int 10h
        pop di
        pop si
        pop bp
        ret
_Vmode  endp

pcpixel proc near
        ror word ptr cs:mask,1
        jc cont
        ret
cont:
        push ax
        push bx
        push cx
        push dx
        mov cx,ax               ; x
        mov dx,bx               ; y
        mov ah,0ch              ; ah = write pixel
        mov al,byte ptr cs:color

        mov bh, 0               ; page 0
        int 10h
        pop dx
        pop cx
        pop bx
        pop ax
        ret
pcpixel endp

_PC_curloc proc far
        push bp
        mov bp,sp
        mov dh, byte ptr [bp+X] ; row number
        mov dl, byte ptr [bp+X+2] ; col number
        mov bh, 0
        mov ah, 2
        int 10h
        pop bp
        ret
_PC_curloc endp

_PC_puts proc far
        push bp
        mov bp,sp
        mov ah,0eh              ; write TTY char
        mov bl,byte ptr cs:color
        mov es,[bp+X+2]         ; segment
        mov bp,[bp+X]           ; offset
puts2:  mov al,es:[bp]
        or al,al
        jz puts3
        int 10h
        inc bp
        jmp short puts2
puts3:  pop bp
        ret
_PC_puts endp

_prog   ends
        end
ov bp,sp
        push ax
        push bx
        push cx
        mov es,c
