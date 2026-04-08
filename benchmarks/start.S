  .section .text
  .align 2
  .globl _start
  .type _start, @function
_start:
  # Initialize stack and global pointer (use explicit auipc/addi to avoid la pseudo optimizations)
  # Use LUI/ADDI sequence to form absolute addresses for sp/gp
  lui   sp, %hi(_stack_top)
  addi  sp, sp, %lo(_stack_top)
  # Set gp to a small-data base (RAM origin + 0x1000)
  # Use a constant LUI so gp is initialized predictably
  lui   gp, 0x1
  addi  gp, gp, 0

  # Copy initialized data from FLASH (LMA) to RAM (VMA)
  la t0, __data_load
  la t1, _sdata
  la t2, _edata
  beq t1, t2, 1f
2:
  lw t3, 0(t0)
  sw t3, 0(t1)
  addi t0, t0, 4
  addi t1, t1, 4
  bne t1, t2, 2b
1:

  # Zero BSS
  la t0, _sbss
  la t1, _ebss
  beq t0, t1, 3f
4:
  sw zero, 0(t0)
  addi t0, t0, 4
  bne t0, t1, 4b
3:

  # Call main
  call main

  # On return, signal host via tohost then hang
  la t0, tohost
  li t1, 1
  sw t1, 0(t0)
hang:
  j hang

  .align 2
  .globl memset
  .type memset, @function
memset:
  # a0=dst, a1=byte value, a2=len
  mv t0, a0
  andi a1, a1, 0xff
1:
  beqz a2, 2f
  sb a1, 0(t0)
  addi t0, t0, 1
  addi a2, a2, -1
  j 1b
2:
  ret
