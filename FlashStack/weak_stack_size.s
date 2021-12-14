    .text
    .type   .BUDDY.CALL_STACK_SIZE_MASK,@object # @CALL_STACK_SIZE_MASK
    .data
    .weak   .BUDDY.CALL_STACK_SIZE_MASK
    .p2align    12
.BUDDY.CALL_STACK_SIZE_MASK:
    .quad   -8388608                # 0xffffffffff800000
    .quad   0x57534E5540455343      # CSE@UNSW
    .quad   0x57534E5540455343      # CSE@UNSW 
    .quad   0x57534E5540455343      # CSE@UNSW     
    .zero   4064
    .size   .BUDDY.CALL_STACK_SIZE_MASK, 4096

    .ident  "GCC: (Ubuntu 7.5.0-3ubuntu1~18.04) 7.5.0"
    .section    .note.GNU-stack,"",@progbits
