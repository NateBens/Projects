global _start
global itoa

section .text
_start:
    ; rbx = atoi(n, buffer)
    mov rdi, n
    mov rsi, buffer
    call itoa
    mov rbx, rax

    ; append a newline
    mov al, 10
    mov [buffer + rbx], al

    ; write(1, buffer, rbx)
    mov rax, 1
    mov rdi, 1
    mov rsi, buffer
    mov rdx, rbx
    inc rdx
    syscall

    ; exit(bytes)
    mov rax, 60
    mov rdi, rbx
    syscall

; itoa(n, buffer) -> bytes written
itoa:
    mov rax, rdi                ; move n to rax to make division easier
    mov rdi, 0                  ; rdi = length of output
    mov r10, 10                 ; base 10
loop:
    ; do a division
    mov rdx, 0
    div r10
    add rdx, '0'                ; add '0'
    mov [rdi + rsi], dl         ; store in the buffer
    inc rdi
    cmp rax, 0
    jg loop

    ; reverse string
    mov rdx, rsi                ; rdx = pointer to beginning
    mov rcx, rsi                ; rcx = pointer to end
    add rcx, rdi
    dec rcx
    jmp reverse_test

reverse_loop:
    mov al, [rdx]
    mov ah, [rcx]
    mov [rcx], al
    mov [rdx], ah
    inc rdx
    dec rcx

reverse_test:
    cmp rdx, rcx
    jl reverse_loop

    mov rax, rdi
    ret

section .bss
    n: equ 2304823487234
    buffer: resb 32
