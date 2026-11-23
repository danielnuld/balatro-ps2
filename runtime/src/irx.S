# audsrv.irx (ps2sdk; build.sh copies it beside the Makefile) inside the ELF, for snd.c to start from memory
    .section .rodata
    .balign 64
    .globl audsrv_irx, audsrv_irx_end
audsrv_irx:
    .incbin "audsrv.irx"
audsrv_irx_end:
