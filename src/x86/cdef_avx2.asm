; Copyright © 2018, VideoLAN and dav1d authors
; Copyright © 2018, Two Orioles, LLC
; All rights reserved.
;
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions are met:
;
; 1. Redistributions of source code must retain the above copyright notice, this
;    list of conditions and the following disclaimer.
;
; 2. Redistributions in binary form must reproduce the above copyright notice,
;    this list of conditions and the following disclaimer in the documentation
;    and/or other materials provided with the distribution.
;
; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
; ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
; WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
; DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
; ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
; (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
; ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
; (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
; SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

%include "config.asm"
%include "ext/x86/x86inc.asm"

%if ARCH_X86_64

%define CDEF_BUFFER_UNITS 8
%define CDEF_BUFFER_Y_STRIDE (CDEF_BUFFER_UNITS * 8 + 8 * 2)

%macro JMP_TABLE 2-*
 %xdefine %1_jmptable %%table
 %xdefine %%base mangle(private_prefix %+ _%1_avx2)
 %%table:
 %rep %0 - 1
    dd %%base %+ .%2 - %%table
  %rotate 1
 %endrep
%endmacro

%macro CDEF_FILTER_JMP_TABLE 1
JMP_TABLE cdef_filter_%1_8bpc, \
    d6k0, d6k1, d7k0, d7k1, \
    d0k0, d0k1, d1k0, d1k1, d2k0, d2k1, d3k0, d3k1, \
    d4k0, d4k1, d5k0, d5k1, d6k0, d6k1, d7k0, d7k1, \
    d0k0, d0k1, d1k0, d1k1
%endmacro

%macro CDEF_PREP_JMP_TABLE 1
JMP_TABLE cdef_prep_%1_8bpc, \
    no_left_right, no_right,           no_left, full, \
    no_left_right, no_right,           no_left, full, \
    no_left_right, no_right,           no_left, full, \
    no_left_right, no_right,           no_left, full, \
    no_left_right, left_skip_no_right, no_left, left_skip_full, \
    no_left_right, left_skip_no_right, no_left, left_skip_full, \
    no_left_right, left_skip_no_right, no_left, left_skip_full, \
    no_left_right, left_skip_no_right, no_left, left_skip_full
%endmacro

SECTION_RODATA 32

pd_47130256:   dd  4,  7,  1,  3,  0,  2,  5,  6
div_table:     dd 840, 420, 280, 210, 168, 140, 120, 105, 420, 210, 140, 105
shufw_6543210x:db 12, 13, 10, 11,  8,  9,  6,  7,  4,  5,  2,  3,  0,  1, 14, 15
shufb_lohi:    db  0,  8,  1,  9,  2, 10,  3, 11,  4, 12,  5, 13,  6, 14,  7, 15
pb_deinterleave:
               db 0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15
pb_128:        times 4 db 128
pw_128:        times 2 dw 128
pw_2048:       times 2 dw 2048

tap_table:
; masks for 8 bit shifts
table_shift_masks:
               dd 0xFFFFFFFF, 0x7F7F7F7F, 0x3F3F3F3F, 0x1F1F1F1F,
               dd 0x0F0F0F0F, 0x07070707, 0x03030303, 0x01010101
; weights
tap_weights:   db  4,  2,  2,  1,  3,  3,  2,  1
tap_table_y:   db -1 * 32 + 1, -2 * 32 + 2
               db  0 * 32 + 1, -1 * 32 + 2
               db  0 * 32 + 1,  0 * 32 + 2
               db  0 * 32 + 1,  1 * 32 + 2
               db  1 * 32 + 1,  2 * 32 + 2
               db  1 * 32 + 0,  2 * 32 + 1
               db  1 * 32 + 0,  2 * 32 + 0
               db  1 * 32 + 0,  2 * 32 - 1
               ; the last 6 are repeats of the first 6 so we don't need to & 7
               db -1 * 32 + 1, -2 * 32 + 2
               db  0 * 32 + 1, -1 * 32 + 2
               db  0 * 32 + 1,  0 * 32 + 2
               db  0 * 32 + 1,  1 * 32 + 2
               db  1 * 32 + 1,  2 * 32 + 2
               db  1 * 32 + 0,  2 * 32 + 1
tap_table_uv:  db -1 * 32 + 2, -2 * 32 + 4
               db  0 * 32 + 2, -1 * 32 + 4
               db  0 * 32 + 2,  0 * 32 + 4
               db  0 * 32 + 2,  1 * 32 + 4
               db  1 * 32 + 2,  2 * 32 + 4
               db  1 * 32 + 0,  2 * 32 + 2
               db  1 * 32 + 0,  2 * 32 + 0
               db  1 * 32 + 0,  2 * 32 - 2
               ; the last 6 are repeats of the first 6 so we don't need to & 7
               db -1 * 32 + 2, -2 * 32 + 4
               db  0 * 32 + 2, -1 * 32 + 4
               db  0 * 32 + 2,  0 * 32 + 4
               db  0 * 32 + 2,  1 * 32 + 4
               db  1 * 32 + 2,  2 * 32 + 4
               db  1 * 32 + 0,  2 * 32 + 2

CDEF_FILTER_JMP_TABLE uv_8x8
CDEF_FILTER_JMP_TABLE uv_4x4
; Share 420 and 422 table
%define cdef_filter_uv_4x8_8bpc_jmptable cdef_filter_uv_4x4_8bpc_jmptable
CDEF_FILTER_JMP_TABLE y

CDEF_PREP_JMP_TABLE uv_444
CDEF_PREP_JMP_TABLE uv_420
; Share 420 and 422 table
%define cdef_prep_uv_422_8bpc_jmptable cdef_prep_uv_420_8bpc_jmptable
CDEF_PREP_JMP_TABLE y

SECTION .text

; IMPLEMENTATION SPECIFIC DETAILS:
;   cdef_filter and cdef_prep allow customization of how the intermediary
; buffer is stored. For avx2, all pixel data is subtracted by 128 (XOR 128),
; and chroma pixels are interleaved.

%macro PREP_REGS 3 ; w, h, uv
    ; off1/2/3[k] [6 total] from [tapq+12+(dir+0/2/6)*2+k]
    mov           dird, r6m  ; load or zero extend register
%if %3 == 1
    lea         tableq, [cdef_filter_uv_%1x%2_8bpc_jmptable]
%else
    lea         tableq, [cdef_filter_y_8bpc_jmptable]
%endif
    lea           dirq, [tableq+ dirq*2*4]
%if %1 == 4
  %define vloop_lines 4
 %if %2 == 4
  DEFINE_ARGS dst1, stride, buf, k, tap, dst2, \
              dir, table, dirjmp
 %else
  DEFINE_ARGS dst1, stride, buf, k, tap, dst2, \
              dir, table, dirjmp, stride3, h
    mov             hd, 2
    lea       stride3q, [strideq*3]
 %endif
%else
 %if %3 == 1
  DEFINE_ARGS dst1, stride, buf, k, tap, dst2, \
              dir, table, dirjmp, h
    mov             hd, 4
  %define vloop_lines 2
 %else
  DEFINE_ARGS dst, stride, buf, k, tap, h, \
              dir, table, dirjmp, stride3
    mov             hd, 2
    lea       stride3q, [strideq*3]
   %define vloop_lines 4
  %endif
%endif
%if %3 == 1
    mov          dst2q, [dst1q+gprsize]
    mov          dst1q, [dst1q]
%endif
%endmacro

%macro LOAD_BLOCK 3-4 0 ; w, h, uv, init_min_max
    mov             kd, 1
    pxor           m15, m15                     ; sum
    pxor           m12, m12
%if %1 == 8 && %2 == 8 && %3 == 1
    movu           xm4, [bufq           ]
    vinserti128     m4, [bufq+buf_stride], 1
%else
    movq           xm4, [bufq+buf_stride*0]
    movq           xm5, [bufq+buf_stride*1]
    vinserti128     m4, [bufq+buf_stride*2], 1
    vinserti128     m5, [bufq+buf_stride*3], 1
    punpcklqdq      m4, m5
%endif
%if %4 == 1
    mova            m7, m4                      ; min
    mova            m8, m4                      ; max
%endif
%endmacro

%macro ACCUMULATE_TAP_BYTE 7-8 0 ; tap_offset, shift, mask, strength
                                 ; mul_tap, w, h, clip
    ; load p0/p1
    movsxd     dirjmpq, [dirq+kq*4+%1*2*4]
    add        dirjmpq, tableq
    call       dirjmpq

%if %8 == 1
    pmaxsb          m7, m5
    pminsb          m8, m5
    pmaxsb          m7, m6
    pminsb          m8, m6
%endif

    ; accumulate sum[m15] over p0/p1
    psubsb          m5, m4
    psubsb          m6, m4
    pabsb           m9, m5
    vpsrlvd        m10, m9, %2  ; emulate 8-bit shift
    pand           m10, %3
    psubusb        m10, %4, m10
    pminub         m10, m9
    psignb         m10, m5
    pabsb           m9, m6
    vpsrlvd        m11, m9, %2  ; emulate 8-bit shift
    pand           m11, %3
    psubusb        m11, %4, m11
    pminub         m11, m9
    psignb         m11, m6
    punpckhbw      m6, m10, m11
    punpcklbw      m10, m11
    pmaddubsw      m6, %5, m6
    pmaddubsw      m10, %5, m10
    paddw          m12, m6
    paddw          m15, m10
%endmacro

%macro ADJUST_PIXEL 7-8 0 ; w, h, uv, zero, pw_2048, pb_128,
                          ; deinterleave (for uv), clip
    pxor            m4, %6
%if %8 == 1
    pxor            m7, %6
    pxor            m8, %6
%endif
%if %1 == 4
    pcmpgtw         m6, %4, m12
    pcmpgtw         m5, %4, m15
    paddw          m12, m6
    paddw          m15, m5
 %if %8 == 1
    punpckhbw       m5, m4, %4
    punpcklbw       m4, %4
 %endif
    pmulhrsw       m12, %5
    pmulhrsw       m15, %5
 %if %8 == 0
    packsswb       m15, m12
    paddb           m4, m15
 %else
    paddw           m5, m12
    paddw           m4, m15
    packuswb        m4, m5 ; clip px in [0x0,0xff]
    pminub          m4, m7
    pmaxub          m4, m8
 %endif
 %if %2 == 4
    lea             kq, [strideq*3]
    %define stride3q kq
 %endif
    pshufb          m4, %7
    vextracti128   xm5, m4, 1
    movd   [dst1q+strideq*0], xm4
    movd   [dst1q+strideq*2], xm5
    pextrd [dst1q+strideq*1], xm4, 1
    pextrd [dst1q+stride3q ], xm5, 1
    pextrd [dst2q+strideq*0], xm4, 2
    pextrd [dst2q+strideq*2], xm5, 2
    pextrd [dst2q+strideq*1], xm4, 3
    pextrd [dst2q+stride3q ], xm5, 3
 %if %2 == 4
    %undef stride3q
 %endif
%else
    pcmpgtw         m6, %4, m12
    pcmpgtw         m5, %4, m15
    paddw          m12, m6
    paddw          m15, m5
 %if %8 == 1
    punpckhbw       m5, m4, %4
    punpcklbw       m4, %4
 %endif
    pmulhrsw       m12, %5
    pmulhrsw       m15, %5
 %if %8 == 0
    packsswb       m15, m12
    paddb           m4, m15
 %else
    paddw           m5, m12
    paddw           m4, m15
    packuswb        m4, m5 ; clip px in [0x0,0xff]
    pminub          m4, m7
    pmaxub          m4, m8
 %endif
 %if %3 == 1
    pshufb          m4, %7
    vextracti128   xm5, m4, 1
    movq   [dst1q        ], xm4
    movq   [dst1q+strideq], xm5
    movhps [dst2q        ], xm4
    movhps [dst2q+strideq], xm5
 %else
    vextracti128   xm5, m4, 1
    movq   [dstq+strideq*0], xm4
    movq   [dstq+strideq*2], xm5
    movhps [dstq+strideq*1], xm4
    movhps [dstq+stride3q ], xm5
  %endif
%endif
%endmacro

%macro BORDER_PREP_REGS 3 ; w, h, uv
    ; off1/2/3[k] [6 total] from [tapq+12+(dir+0/2/6)*2+k]
    mov           dird, r6m
%if %3 == 1
    lea           dirq, [tableq+dirq*2+tap_table_uv-tap_table]
    DEFINE_ARGS dst1, stride, k, h, tap, stk, dir, off, dst2
    mov             hd, %1*%2*4/mmsize
    mov          dst2q, [dst1q+gprsize]
    mov          dst1q, [dst1q]
%else
    lea           dirq, [tableq+dirq*2+tap_table_y-tap_table]
    DEFINE_ARGS dst, stride, k, h, tap, stk, dir, off
    mov             hd, %1*%2*2/mmsize
%endif
    lea           stkq, [px]
    pxor           m11, m11
%endmacro

%macro BORDER_LOAD_BLOCK 3-4 0 ; w, h, uv, init_min_max
    mov             kd, 1
%if %1 == 8 && %3 == 1
    mova            m4, [stkq]
%else
    mova           xm4, [stkq+64*0]             ; px
    vinserti128     m4, [stkq+64*1], 1
%endif
    pxor           m15, m15                     ; sum
%if %4 == 1
    mova            m7, m4                      ; max
    mova            m8, m4                      ; min
%endif
%endmacro

%macro ACCUMULATE_TAP_WORD 8-9 0 ; tap_offset, shift, mask, strength
                                 ; mul_tap, w, h, uv, clip
    ; load p0/p1
    movsx         offq, byte [dirq+kq+%1]       ; off1
%if %6 == 8 && %8 == 1
    movu            m5, [stkq+offq*2]
%else
    movu           xm5, [stkq+offq*2+64*0]
    vinserti128     m5, [stkq+offq*2+64*1], 1
%endif
    neg           offq                          ; -off1
%if %6 == 8 && %8 == 1
    movu            m6, [stkq+offq*2]
%else
    movu           xm6, [stkq+offq*2+64*0]      ; p1
    vinserti128     m6, [stkq+offq*2+64*1], 1
%endif
%if %9 == 1
    ; out of bounds values are set to a value that is a both a large unsigned
    ; value and a negative signed value.
    ; use signed max and unsigned min to remove them
    pmaxsw          m7, m5                      ; max after p0
    pminuw          m8, m5                      ; min after p0
    pmaxsw          m7, m6                      ; max after p1
    pminuw          m8, m6                      ; min after p1
%endif

    ; accumulate sum[m15] over p0/p1
    ; calculate difference before converting
    psubw           m5, m4                      ; diff_p0(p0 - px)
    psubw           m6, m4                      ; diff_p1(p1 - px)

    ; convert to 8-bits with signed saturation
    ; saturating to large diffs has no impact on the results
    packsswb        m5, m6

    ; group into pairs so we can accumulate using maddubsw
    pshufb          m5, m12
    pabsb           m9, m5
    psignb         m10, %5, m5
    psrlw           m5, m9, %2                  ; emulate 8-bit shift
    pand            m5, %3
    psubusb         m5, %4, m5

    ; use unsigned min since abs diff can equal 0x80
    pminub          m5, m9
    pmaddubsw       m5, m10
    paddw          m15, m5
%endmacro

%macro BORDER_ADJUST_PIXEL 5-6 0 ; w, h, uv, pw_2048, deinterleave (for uv),
                                 ; clip
    pcmpgtw         m9, m11, m15
    paddw          m15, m9
    pmulhrsw       m15, %4
    paddw           m4, m15
%if %6 == 1
    pminsw          m4, m7
    pmaxsw          m4, m8
%endif
    vextracti128   xm5, m4, 1
    packuswb       xm4, xm5
%if %3 == 1
    pshufb         xm4, %5
 %if %1 == 4
    movd   [dst1q+strideq*0], xm4
    pextrd [dst1q+strideq*1], xm4, 1
    pextrd [dst2q+strideq*0], xm4, 2
    pextrd [dst2q+strideq*1], xm4, 3
 %elif %1 == 8
    movq       [dst1q], xm4
    movhps     [dst2q], xm4
 %endif
%else
    movq   [dstq+strideq*0], xm4
    movhps [dstq+strideq*1], xm4
%endif
%endmacro

%macro CDEF_FILTER 3 ; w, h, uv
INIT_YMM avx2
%if %3 == 1
cglobal cdef_filter_uv_%1x%2_8bpc, 6, 10, 0, dst, stride, buf, cbx, pri, \
                                             sec, dir, damping, edge
%define buf_stride (CDEF_BUFFER_Y_STRIDE * 2 * %1 / 8)
%if %1 == 8
    add           cbxq, cbxq ; double since cbxq*8 is invalid address offset
%endif
    lea           bufq, [bufq+cbxq*8+buf_stride*2+8]
%else
cglobal cdef_filter_y_8bpc, 6, 9, 0, dst, stride, buf, cbx, pri, sec, dir, \
                                     damping, edge
%define buf_stride CDEF_BUFFER_Y_STRIDE
    lea           bufq, [bufq+cbxq*%1+buf_stride*2+8]
%endif
%assign stack_offset_entry stack_offset
%assign regs_used_entry regs_used
    mov          edged, edgem
    cmp          edged, 0xf
    jne .border_block

%if %3 != 1   ; !uv
    PUSH           r9
 %assign regs_used 10
%elif %1 == 4 && %2 == 8 ; uv && w == 4 && h == 8
    PUSH           r10
 %assign regs_used 11
%endif

%if STACK_ALIGNMENT < 32
    PUSH  r%+regs_used
 %assign regs_used regs_used+1
%endif
    ALLOC_STACK 8*2, 16

 DEFINE_ARGS dst, stride, buf, zero, pri, secdmp, dir, damping, pridmp
    movifnidn dampingd, dampingm
    xor          zerod, zerod
    sub       dampingd, 31
    test          prid, prid
    jz .sec_only
    movd           xm0, prid
    lzcnt      pridmpd, prid
    add        pridmpd, dampingd
    cmovs      pridmpd, zerod
    mov        [rsp+0], pridmpq                 ; pri_shift
    test       secdmpd, secdmpd
    jz .pri_only
    movd           xm1, secdmpd
    lzcnt      secdmpd, secdmpd
    add        secdmpd, dampingd
    mov        [rsp+8], secdmpq                 ; sec_shift

 DEFINE_ARGS dst, stride, buf, _, pri, secdmp, dir, table, pridmp
    lea         tableq, [tap_table]
    vpbroadcastd   m13, [tableq+pridmpq*4]      ; pri_shift_mask
    vpbroadcastd   m14, [tableq+secdmpq*4]      ; sec_shift_mask

    ; pri/sec_taps[k] [4 total]
 DEFINE_ARGS dst, stride, buf, _, tap, _, dir, table
    vpbroadcastb    m0, xm0                     ; pri_strength
    vpbroadcastb    m1, xm1                     ; sec_strength
    and           tapd, 1
    ; pri_taps/sec_taps
    lea           tapq, [tableq+tapq*4+tap_weights-tap_table]

    PREP_REGS       %1, %2, %3
%if %2 == 8
.v_loop:
%endif
    LOAD_BLOCK      %1, %2, %3, 1
.k_loop:
    vpbroadcastb    m2, [tapq+kq]                     ; pri_taps
    vpbroadcastd    m3, [rsp+0]                       ; pri_shift
    ACCUMULATE_TAP_BYTE 2, m3, m13, m0, m2, %1, %2, 1 ; dir + 0
    vpbroadcastb    m2, [tapq+kq+2]                   ; sec_taps
    vpbroadcastd    m3, [rsp+8]                       ; sec_shift
    ACCUMULATE_TAP_BYTE 4, m3, m14, m1, m2, %1, %2, 1 ; dir + 2
    ACCUMULATE_TAP_BYTE 0, m3, m14, m1, m2, %1, %2, 1 ; dir - 2
    dec             kq
    jge .k_loop

    vpbroadcastd   m11, [pb_128]
%if %3 == 1
    vbroadcasti128  m2, [pb_deinterleave]
%endif
    vpbroadcastd   m10, [pw_2048]
    pxor            m9, m9
    ADJUST_PIXEL    %1, %2, %3, m9, m10, m11, m2, 1
%if %2 == 8
%if %3 == 1
    lea          dst1q, [dst1q+strideq*vloop_lines]
    lea          dst2q, [dst2q+strideq*vloop_lines]
%else
    lea           dstq, [dstq+strideq*vloop_lines]
%endif
    add           bufq, buf_stride*vloop_lines
    dec             hd
    jg .v_loop
%endif
    RET

.pri_only:
 DEFINE_ARGS dst, stride, buf, _, pri, _, dir, table, pridmp
    lea         tableq, [tap_table]
    vpbroadcastd   m13, [tableq+pridmpq*4]      ; pri_shift_mask
    ; pri/sec_taps[k] [4 total]
 DEFINE_ARGS dst, stride, buf, _, tap, _, dir, table
    vpbroadcastb    m0, xm0                     ; pri_strength
    and           tapd, 1
    ; pri_taps
    lea           tapq, [tableq+tapq*4+tap_weights-tap_table]
    PREP_REGS       %1, %2, %3
    vpbroadcastd    m1, [pb_128]
%if %2 == 8
.pri_v_loop:
%endif
    LOAD_BLOCK      %1, %2, %3
.pri_k_loop:
    vpbroadcastb    m2, [tapq+kq]                  ; pri_taps
    vpbroadcastd    m3, [rsp+0]                    ; pri_shift
    ACCUMULATE_TAP_BYTE 2, m3, m13, m0, m2, %1, %2 ; dir + 0
    dec             kq
    jge .pri_k_loop

    vpbroadcastd    m3, [pw_2048]
%if %3 == 1
    vbroadcasti128  m2, [pb_deinterleave]
%endif
    pxor            m11, m11
    ADJUST_PIXEL    %1, %2, %3, m11, m3, m1, m2
%if %2 == 8
%if %3 == 1
    lea          dst1q, [dst1q+strideq*vloop_lines]
    lea          dst2q, [dst2q+strideq*vloop_lines]
%else
    lea           dstq, [dstq+strideq*vloop_lines]
%endif
    add           bufq, buf_stride*vloop_lines
    dec             hd
    jg .pri_v_loop
%endif
    RET

.sec_only:
 DEFINE_ARGS dst, stride, buf, _, _, secdmp, dir, damping
    movd           xm1, secdmpd
    lzcnt      secdmpd, secdmpd
    add        secdmpd, dampingd
    mov        [rsp+8], secdmpq                 ; sec_shift
 DEFINE_ARGS dst, stride, buf, _, _, secdmp, dir, table
    lea         tableq, [tap_table]
    vpbroadcastd   m14, [tableq+secdmpq*4]      ; sec_shift_mask
    ; pri/sec_taps[k] [4 total]
 DEFINE_ARGS dst, stride, buf, _, tap, _, dir, table
    vpbroadcastb    m1, xm1                     ; sec_strength
    ; sec_taps
    lea           tapq, [tableq+2+tap_weights-tap_table]
    PREP_REGS       %1, %2, %3
    vpbroadcastd    m0, [pb_128]
%if %2 == 8
.sec_v_loop:
%endif
    LOAD_BLOCK      %1, %2, %3
.sec_k_loop:
    vpbroadcastb    m2, [tapq+kq]                  ; sec_taps
    vpbroadcastd    m3, [rsp+8]                    ; sec_shift
    ACCUMULATE_TAP_BYTE 4, m3, m14, m1, m2, %1, %2 ; dir + 2
    ACCUMULATE_TAP_BYTE 0, m3, m14, m1, m2, %1, %2 ; dir - 2
    dec             kq
    jge .sec_k_loop

    vpbroadcastd    m2, [pw_2048]
%if %3 == 1
    vbroadcasti128  m3, [pb_deinterleave]
%endif
    pxor           m11, m11
    ADJUST_PIXEL    %1, %2, %3, m11, m2, m0, m3
%if %2 == 8
%if %3 == 1
    lea          dst1q, [dst1q+strideq*vloop_lines]
    lea          dst2q, [dst2q+strideq*vloop_lines]
%else
    lea           dstq, [dstq+strideq*vloop_lines]
%endif
    add           bufq, buf_stride*vloop_lines
    dec             hd
    jg .sec_v_loop
%endif
    RET

; Share 4 wide load code with both heights.
%if !(%1 == 4 && %2 == 8)
.d0k0:
%if %1 == 4
    movq           xm5, [bufq-buf_stride*1+2]
    vbroadcasti128 m10, [bufq+buf_stride*1-2]
    vbroadcasti128 m11, [bufq+buf_stride*2-2]
    movhps         xm5, [bufq+buf_stride*0+2]
    vinserti128     m6, m10, [bufq+buf_stride*3-2], 1
    vinserti128     m9, m11, [bufq+buf_stride*4-2], 1
    shufps         m10, m11, q2121
    punpcklqdq      m6, m9
    vpblendd        m5, m10, 0xF0
%elif %3 == 1 ; 8x8 && uv
    ;    y,  x
    ; {( 0,  0), (+1,  0)}

    ; {(-1, +1), ( 0, +1)}
    ; {(+1, -1), (+2, -1)}
    movu           xm5, [bufq-buf_stride*1+2]
    movu           xm6, [bufq+buf_stride*1-2]
    vinserti128     m5, [bufq+buf_stride*0+2], 1
    vinserti128     m6, [bufq+buf_stride*2-2], 1
%else
    movq           xm5, [bufq-buf_stride*1+1]
    vbroadcasti128 m10, [bufq+buf_stride*1-1]
    vbroadcasti128 m11, [bufq+buf_stride*2-1]
    movhps         xm5, [bufq+buf_stride*0+1]
    vinserti128     m6, m10, [bufq+buf_stride*3-1], 1
    vinserti128     m9, m11, [bufq+buf_stride*4-1], 1
    psrldq         m10, 2
    psrldq         m11, 2
    punpcklqdq      m6, m9
    punpcklqdq     m10, m11
    vpblendd        m5, m10, 0xF0
%endif
    ret
.d1k0:
.d2k0:
.d3k0:
%if %1 == 4
    movu           xm5, [bufq+buf_stride*0-2]
    movu           xm9, [bufq+buf_stride*1-2]
    vinserti128     m5, [bufq+buf_stride*2-2], 1
    vinserti128     m9, [bufq+buf_stride*3-2], 1
    punpcklqdq      m6, m5, m9
    shufps          m5, m9, q2121
%elif %3 == 1
    ;    y,  x
    ; {( 0,  0), (+1,  0)}

    ; {( 0, +1), (+1, +1)}
    ; {( 0, -1), (+1, -1)}
    movu            m6, [bufq+buf_stride*0-2]
    movu            m9, [bufq+buf_stride*1-2]
    vperm2i128      m5, m6, m9, 0x31 ; +6, +7, +8, +9
    vinserti128     m6, xm9, 1       ; -2, -1,  0,  ...  +5
    palignr         m5, m6, 4
%else
    movu           xm5, [bufq+buf_stride*0-1]
    movu           xm9, [bufq+buf_stride*1-1]
    vinserti128     m5, [bufq+buf_stride*2-1], 1
    vinserti128     m9, [bufq+buf_stride*3-1], 1
    punpcklqdq      m6, m5, m9
    psrldq          m5, 2
    psrldq          m9, 2
    punpcklqdq      m5, m9
%endif
    ret
.d4k0:
%if %1 == 4
    movq           xm6, [bufq-buf_stride*1-2]
    vbroadcasti128  m5, [bufq+buf_stride*1-2]
    vbroadcasti128  m9, [bufq+buf_stride*2-2]
    movhps         xm6, [bufq+buf_stride*0-2]
    punpcklqdq     m10, m5, m9
    vinserti128     m5, [bufq+buf_stride*3-2], 1
    vinserti128     m9, [bufq+buf_stride*4-2], 1
    vpblendd        m6, m10, 0xF0
    shufps          m5, m9, q2121
%elif %3 == 1
    ;    y,  x
    ; {( 0,  0), (+1,  0)}

    ; {(+1, +1), (+2, +1)}
    ; {(-1, -1), ( 0, -1)}
    movu           xm5, [bufq+buf_stride*1+2]
    movu           xm6, [bufq-buf_stride*1-2]
    vinserti128     m5, [bufq+buf_stride*2+2], 1
    vinserti128     m6, [bufq+buf_stride*0-2], 1
%else
    movq           xm6, [bufq-buf_stride*1-1]
    vbroadcasti128  m5, [bufq+buf_stride*1-1]
    vbroadcasti128  m9, [bufq+buf_stride*2-1]
    movhps         xm6, [bufq+buf_stride*0-1]
    punpcklqdq     m10, m5, m9
    vinserti128     m5, [bufq+buf_stride*3-1], 1
    vinserti128     m9, [bufq+buf_stride*4-1], 1
    vpblendd        m6, m10, 0xF0
    psrldq          m5, 2
    psrldq          m9, 2
    punpcklqdq      m5, m9
%endif
    ret
.d5k0:
.d6k0:
.d7k0:
%if %1 == 4
    movq           xm6, [bufq-buf_stride*1]
    movq           xm5, [bufq+buf_stride*1]
    movq           xm9, [bufq+buf_stride*3]
    movhps         xm6, [bufq+buf_stride*0]
    movhps         xm5, [bufq+buf_stride*2]
    movhps         xm9, [bufq+buf_stride*4]
    vinserti128     m6, xm5, 1
    vinserti128     m5, xm9, 1
%elif %3 == 1
    ;    y,  x
    ; {( 0,  0), (+1,  0)}

    ; {(+1,  0), (+2,  0)}
    ; {(-1,  0), ( 0,  0)}
    vperm2i128      m9, m4, m4, 0x01
    vinserti128     m5, m9, [bufq+buf_stride*2], 1
    vinserti128     m6, m9, [bufq-buf_stride*1], 0
%else
    movq           xm6, [bufq-buf_stride*1]
    movq           xm5, [bufq+buf_stride*1]
    movq           xm9, [bufq+buf_stride*3]
    movhps         xm6, [bufq+buf_stride*0]
    movhps         xm5, [bufq+buf_stride*2]
    movhps         xm9, [bufq+buf_stride*4]
    vinserti128     m6, xm5, 1
    vinserti128     m5, xm9, 1
%endif
    ret
.d0k1:
%if %1 == 4
    movq           xm6, [bufq+buf_stride*2-4]
    movq           xm9, [bufq+buf_stride*3-4]
    movq           xm5, [bufq-buf_stride*2+4]
    movq          xm10, [bufq-buf_stride*1+4]
    vinserti128     m6, [bufq+buf_stride*4-4], 1
    vinserti128     m9, [bufq+buf_stride*5-4], 1
    vinserti128     m5, [bufq+buf_stride*0+4], 1
    vinserti128    m10, [bufq+buf_stride*1+4], 1
    punpcklqdq      m6, m9
    punpcklqdq      m5, m10
%elif %3 == 1
    ;    y,  x
    ; {( 0,  0), (+1,  0)}

    ; {(-2, +2), (-1, +2)}
    ; {(+2, -2), (+3, -2)}
    movu           xm5, [bufq-buf_stride*2+4]
    movu           xm6, [bufq+buf_stride*2-4]
    vinserti128     m5, [bufq-buf_stride*1+4], 1
    vinserti128     m6, [bufq+buf_stride*3-4], 1
%else
    movq           xm6, [bufq+buf_stride*2-2]
    movq           xm9, [bufq+buf_stride*3-2]
    movq           xm5, [bufq-buf_stride*2+2]
    movq          xm10, [bufq-buf_stride*1+2]
    vinserti128     m6, [bufq+buf_stride*4-2], 1
    vinserti128     m9, [bufq+buf_stride*5-2], 1
    vinserti128     m5, [bufq+buf_stride*0+2], 1
    vinserti128    m10, [bufq+buf_stride*1+2], 1
    punpcklqdq      m6, m9
    punpcklqdq      m5, m10
%endif
    ret
.d1k1:
%if %1 == 4
    movq           xm5, [bufq-buf_stride*1+4]
    vbroadcasti128  m6, [bufq+buf_stride*1-4]
    vbroadcasti128  m9, [bufq+buf_stride*2-4]
    movhps         xm5, [bufq+buf_stride*0+4]
    shufps         m10, m6, m9, q3232            ; +2 +3 +4 +5
    vinserti128     m6, [bufq+buf_stride*3-4], 1
    vinserti128     m9, [bufq+buf_stride*4-4], 1
    vpblendd        m5, m10, 0xF0
    punpcklqdq      m6, m9
%elif %3 == 1
    ;    y,  x
    ; {( 0,  0), (+1,  0)}

    ; {(-1, +2), (-0, +2)}
    ; {(+1, -2), (+2, -2)}
    movu           xm5, [bufq-buf_stride*1+4]
    movu           xm6, [bufq+buf_stride*1-4]
    vinserti128     m5, [bufq+buf_stride*0+4], 1
    vinserti128     m6, [bufq+buf_stride*2-4], 1
%else
    movq           xm5, [bufq-buf_stride*1+2]
    vbroadcasti128  m6, [bufq+buf_stride*1-2]
    vbroadcasti128  m9, [bufq+buf_stride*2-2]
    movhps         xm5, [bufq+buf_stride*0+2]
    shufps         m10, m6, m9, q2121
    vinserti128     m6, [bufq+buf_stride*3-2], 1
    vinserti128     m9, [bufq+buf_stride*4-2], 1
    vpblendd        m5, m10, 0xF0
    punpcklqdq      m6, m9
%endif
    ret
.d2k1:
%if %1 == 4
    movu           xm5, [bufq+buf_stride*0-4]
    movu           xm9, [bufq+buf_stride*1-4]
    vinserti128     m5, [bufq+buf_stride*2-4], 1
    vinserti128     m9, [bufq+buf_stride*3-4], 1
    shufps          m6, m5, m9, q1010
    shufps          m5, m9, q3232
%elif %3 == 1
    ;    y,  x
    ; {( 0,  0), (+1,  0)}

    ; {( 0, +1), (+1, +2)}
    ; {( 0, -2), (+1, -2)}
    movu            m5, [bufq+buf_stride*0-4]
    movu            m9, [bufq+buf_stride*1-4]
    vperm2i128      m6, m5, m9, 0x31 ; +4, +5, +6, +7
    vinserti128     m5, xm9, 1       ; -4, -3, -2,  ...  +3
    palignr         m6, m5, 8
%else
    movu           xm5, [bufq+buf_stride*0-2]
    movu           xm9, [bufq+buf_stride*1-2]
    vinserti128     m5, [bufq+buf_stride*2-2], 1
    vinserti128     m9, [bufq+buf_stride*3-2], 1
    shufps          m6, m5, m9, q1010
    shufps          m5, m9, q2121
%endif
    ret
.d3k1:
%if %1 == 4
    movq           xm6, [bufq-buf_stride*1-4]
    vbroadcasti128  m5, [bufq+buf_stride*1-4]
    vbroadcasti128 m10, [bufq+buf_stride*2-4]
    movhps         xm6, [bufq+buf_stride*0-4]
    punpcklqdq      m9, m5, m10
    vinserti128     m5, [bufq+buf_stride*3-4], 1
    vinserti128    m10, [bufq+buf_stride*4-4], 1
    vpblendd        m6, m9, 0xF0
    shufps          m5, m10, q3232
%elif %3 == 1
    ;    y,  x
    ; {( 0,  0), (+1,  0)}

    ; {(+1, +2), (+2, +2)}
    ; {(-1, -2), (+0, -2)}
    movu           xm5, [bufq+buf_stride*1+4]
    movu           xm6, [bufq-buf_stride*1-4]
    vinserti128     m5, [bufq+buf_stride*2+4], 1
    vinserti128     m6, [bufq+buf_stride*0-4], 1
%else
    movq           xm6, [bufq-buf_stride*1-2]
    vbroadcasti128  m5, [bufq+buf_stride*1-2]
    vbroadcasti128 m10, [bufq+buf_stride*2-2]
    movhps         xm6, [bufq+buf_stride*0-2]
    punpcklqdq      m9, m5, m10
    vinserti128     m5, [bufq+buf_stride*3-2], 1
    vinserti128    m10, [bufq+buf_stride*4-2], 1
    vpblendd        m6, m9, 0xF0
    shufps          m5, m10, q2121
%endif
    ret
.d4k1:
%if %1 == 4
    movq           xm6, [bufq-buf_stride*2-4]
    movq           xm9, [bufq-buf_stride*1-4]
    movq           xm5, [bufq+buf_stride*2+4]
    movq          xm10, [bufq+buf_stride*3+4]
    vinserti128     m6, [bufq+buf_stride*0-4], 1
    vinserti128     m9, [bufq+buf_stride*1-4], 1
    vinserti128     m5, [bufq+buf_stride*4+4], 1
    vinserti128    m10, [bufq+buf_stride*5+4], 1
    punpcklqdq      m6, m9
    punpcklqdq      m5, m10
%elif %3 == 1
    ;    y,  x
    ; {( 0,  0), (+1,  0)}

    ; {(+2, +2), (+3, +2)}
    ; {(-2, -2), (-1, -2)}
    movu           xm5, [bufq+buf_stride*2+4]
    movu           xm6, [bufq-buf_stride*2-4]
    vinserti128     m5, [bufq+buf_stride*3+4], 1
    vinserti128     m6, [bufq-buf_stride*1-4], 1
%else
    movq           xm6, [bufq-buf_stride*2-2]
    movq           xm9, [bufq-buf_stride*1-2]
    movq           xm5, [bufq+buf_stride*2+2]
    movq          xm10, [bufq+buf_stride*3+2]
    vinserti128     m6, [bufq+buf_stride*0-2], 1
    vinserti128     m9, [bufq+buf_stride*1-2], 1
    vinserti128     m5, [bufq+buf_stride*4+2], 1
    vinserti128    m10, [bufq+buf_stride*5+2], 1
    punpcklqdq      m6, m9
    punpcklqdq      m5, m10
%endif
    ret
.d5k1:
%if %1 == 4
    movq           xm6, [bufq-buf_stride*2-2]
    movq           xm9, [bufq-buf_stride*1-2]
    movq           xm5, [bufq+buf_stride*2+2]
    movq          xm10, [bufq+buf_stride*3+2]
    vinserti128     m6, [bufq+buf_stride*0-2], 1
    vinserti128     m9, [bufq+buf_stride*1-2], 1
    vinserti128     m5, [bufq+buf_stride*4+2], 1
    vinserti128    m10, [bufq+buf_stride*5+2], 1
    punpcklqdq      m6, m9
    punpcklqdq      m5, m10
%elif %3 == 1
    ;    y,  x
    ; {( 0,  0), (+1,  0)}

    ; {(+2, +1), (+3, +1)}
    ; {(-2, -1), (-1, -1)}
    movu           xm5, [bufq+buf_stride*2+2]
    movu           xm6, [bufq-buf_stride*2-2]
    vinserti128     m5, [bufq+buf_stride*3+2], 1
    vinserti128     m6, [bufq-buf_stride*1-2], 1
%else
    movq           xm6, [bufq-buf_stride*2-1]
    movq           xm9, [bufq-buf_stride*1-1]
    movq           xm5, [bufq+buf_stride*2+1]
    movq          xm10, [bufq+buf_stride*3+1]
    vinserti128     m6, [bufq+buf_stride*0-1], 1
    vinserti128     m9, [bufq+buf_stride*1-1], 1
    vinserti128     m5, [bufq+buf_stride*4+1], 1
    vinserti128    m10, [bufq+buf_stride*5+1], 1
    punpcklqdq      m6, m9
    punpcklqdq      m5, m10
%endif
    ret
.d6k1:
%if %1 == 4
    movq           xm5, [bufq+buf_stride*4]
    movq           xm6, [bufq-buf_stride*2]
    movhps         xm5, [bufq+buf_stride*5]
    movhps         xm6, [bufq-buf_stride*1]
    vperm2i128      m9, m4, m4, 0x01        ; +2 +3 +0 +1
    vinserti128     m5, m9, xm5, 1
    vpblendd        m6, m9, 0xF0
%elif %3 == 1
    ;    y,  x
    ; {( 0,  0), (+1,  0)}

    ; {(+2,  0), (+3,  0)}
    ; {(-2,  0), (-1,  0)}
    movu           xm5, [bufq+buf_stride*2]
    movu           xm6, [bufq-buf_stride*2]
    vinserti128     m5, [bufq+buf_stride*3], 1
    vinserti128     m6, [bufq-buf_stride*1], 1
%else
    movq           xm5, [bufq+buf_stride*4]
    movq           xm6, [bufq-buf_stride*2]
    movhps         xm5, [bufq+buf_stride*5]
    movhps         xm6, [bufq-buf_stride*1]
    vperm2i128      m9, m4, m4, 0x01        ; +2 +3 +0 +1
    vinserti128     m5, m9, xm5, 1
    vpblendd        m6, m9, 0xF0
%endif
    ret
.d7k1:
%if %1 == 4
    movq           xm5, [bufq+buf_stride*2-2]
    movq           xm9, [bufq+buf_stride*4-2]
    movq           xm6, [bufq-buf_stride*2+2]
    movq          xm10, [bufq+buf_stride*0+2]
    movhps         xm5, [bufq+buf_stride*3-2]
    movhps         xm9, [bufq+buf_stride*5-2]
    movhps         xm6, [bufq-buf_stride*1+2]
    movhps        xm10, [bufq+buf_stride*1+2]
    vinserti128     m5, xm9, 1
    vinserti128     m6, xm10, 1
%elif %3 == 1
    ;    y,  x
    ; {( 0,  0), (+1,  0)}

    ; {(+2, -1), (+3, -1)}
    ; {(-2, +1), (-1, +1)}
    movu           xm5, [bufq+buf_stride*2-2]
    movu           xm6, [bufq-buf_stride*2+2]
    vinserti128     m5, [bufq+buf_stride*3-2], 1
    vinserti128     m6, [bufq-buf_stride*1+2], 1
%else
    movq           xm5, [bufq+buf_stride*2-1]
    movq           xm9, [bufq+buf_stride*4-1]
    movq           xm6, [bufq-buf_stride*2+1]
    movq          xm10, [bufq+buf_stride*0+1]
    movhps         xm5, [bufq+buf_stride*3-1]
    movhps         xm9, [bufq+buf_stride*5-1]
    movhps         xm6, [bufq-buf_stride*1+1]
    movhps        xm10, [bufq+buf_stride*1+1]
    vinserti128     m5, xm9, 1
    vinserti128     m6, xm10, 1
%endif
    ret
%endif ; if !(%1 == 4 && %2 == 8)

.border_block:
 DEFINE_ARGS dst, stride, buf, _, pri, sec, dir, damping, edge
%define rstk rsp
%assign stack_offset stack_offset_entry
%assign regs_used regs_used_entry
%if STACK_ALIGNMENT < 32
    PUSH  r%+regs_used
 %assign regs_used regs_used+1
%endif
    ALLOC_STACK 2*16+(%2+4)*64, 16
%define px rsp+2*16+2*64

    pcmpeqw        m14, m14
    psllw          m14, 15                  ; 0x8000
%if %3 == 1 && %1 == 8
    vpbroadcastd   m15, [pb_128]
    pxor           m13, m13
%else
    psrlw          m15, m14, 8              ; 0x0080
%endif

    ; prepare 16 bit buffer on the stack

    ; read center/right/left/top/bottom from the input buffer then rewrite
    ;   out of bounds regions with 0x8000
%if %1 == 4
    pmovzxbw        m1, [bufq-buf_stride*2-4]
    pmovzxbw        m2, [bufq-buf_stride*1-4]
    pmovzxbw        m3, [bufq+buf_stride*0-4]
    pmovzxbw        m4, [bufq+buf_stride*1-4]
    pxor            m1, m15
    pxor            m2, m15
    pxor            m3, m15
    pxor            m4, m15
    movu     [px-2*64-8], m1
    movu     [px-1*64-8], m2
    movu     [px+0*64-8], m3
    movu     [px+1*64-8], m4
    pmovzxbw        m1, [bufq+buf_stride*2-4]
    pmovzxbw        m2, [bufq+buf_stride*3-4]
    pmovzxbw        m3, [bufq+buf_stride*4-4]
    pmovzxbw        m4, [bufq+buf_stride*5-4]
    pxor            m1, m15
    pxor            m2, m15
    pxor            m3, m15
    pxor            m4, m15
    movu     [px+2*64-8], m1
    movu     [px+3*64-8], m2
    movu     [px+4*64-8], m3
    movu     [px+5*64-8], m4
 %if %2 == 8
    pmovzxbw        m1, [bufq+buf_stride*6-4]
    pmovzxbw        m2, [bufq+buf_stride*7-4]
    pmovzxbw        m3, [bufq+buf_stride*8-4]
    pmovzxbw        m4, [bufq+buf_stride*9-4]
    pxor            m1, m15
    pxor            m2, m15
    pxor            m3, m15
    pxor            m4, m15
    movu     [px+6*64-8], m1
    movu     [px+7*64-8], m2
    movu     [px+8*64-8], m3
    movu     [px+9*64-8], m4
 %endif
%else
 %if %3 == 1
    pxor            m1, m15, [bufq-buf_stride*2-4]
    pxor            m2, m15, [bufq-buf_stride*1-4]
    pxor            m3, m15, [bufq+buf_stride*0-4]
    pxor            m4, m15, [bufq+buf_stride*1-4]
    pmovzxbw        m5, xm1
    pmovzxbw        m6, xm2
    punpcklbw       m1, m13
    punpcklbw       m2, m13
    movu         [px-2*64-8   ], m5
    vextracti128 [px-2*64-8+32], m1, 1
    movu         [px-1*64-8   ], m6
    vextracti128 [px-1*64-8+32], m2, 1
    pmovzxbw        m5, xm3
    pmovzxbw        m6, xm4
    punpcklbw       m3, m13
    punpcklbw       m4, m13
    movu         [px+0*64-8   ], m5
    vextracti128 [px+0*64-8+32], m3, 1
    movu         [px+1*64-8   ], m6
    vextracti128 [px+1*64-8+32], m4, 1
    pxor            m1, m15, [bufq+buf_stride*2-4]
    pxor            m2, m15, [bufq+buf_stride*3-4]
    pxor            m3, m15, [bufq+buf_stride*4-4]
    pxor            m4, m15, [bufq+buf_stride*5-4]
    pmovzxbw        m5, xm1
    pmovzxbw        m6, xm2
    punpcklbw       m1, m13
    punpcklbw       m2, m13
    movu         [px+2*64-8   ], m5
    vextracti128 [px+2*64-8+32], m1, 1
    movu         [px+3*64-8   ], m6
    vextracti128 [px+3*64-8+32], m2, 1
    pmovzxbw        m5, xm3
    pmovzxbw        m6, xm4
    punpcklbw       m3, m13
    punpcklbw       m4, m13
    movu         [px+4*64-8   ], m5
    vextracti128 [px+4*64-8+32], m3, 1
    movu         [px+5*64-8   ], m6
    vextracti128 [px+5*64-8+32], m4, 1
    pxor            m1, m15, [bufq+buf_stride*6-4]
    pxor            m2, m15, [bufq+buf_stride*7-4]
    pxor            m3, m15, [bufq+buf_stride*8-4]
    pxor            m4, m15, [bufq+buf_stride*9-4]
    pmovzxbw        m5, xm1
    pmovzxbw        m6, xm2
    punpcklbw       m1, m13
    punpcklbw       m2, m13
    movu         [px+6*64-8   ], m5
    vextracti128 [px+6*64-8+32], m1, 1
    movu         [px+7*64-8   ], m6
    vextracti128 [px+7*64-8+32], m2, 1
    pmovzxbw        m5, xm3
    pmovzxbw        m6, xm4
    punpcklbw       m3, m13
    punpcklbw       m4, m13
    movu         [px+8*64-8   ], m5
    vextracti128 [px+8*64-8+32], m3, 1
    movu         [px+9*64-8   ], m6
    vextracti128 [px+9*64-8+32], m4, 1
 %else
    pmovzxbw        m1, [bufq-buf_stride*2-(%1/2)]
    pmovzxbw        m2, [bufq-buf_stride*1-(%1/2)]
    pmovzxbw        m3, [bufq+buf_stride*0-(%1/2)]
    pmovzxbw        m4, [bufq+buf_stride*1-(%1/2)]
    pxor            m1, m15
    pxor            m2, m15
    pxor            m3, m15
    pxor            m4, m15
    movu  [px-2*64-%1], m1
    movu  [px-1*64-%1], m2
    movu  [px+0*64-%1], m3
    movu  [px+1*64-%1], m4
    pmovzxbw        m1, [bufq+buf_stride*2-(%1/2)]
    pmovzxbw        m2, [bufq+buf_stride*3-(%1/2)]
    pmovzxbw        m3, [bufq+buf_stride*4-(%1/2)]
    pmovzxbw        m4, [bufq+buf_stride*5-(%1/2)]
    pxor            m1, m15
    pxor            m2, m15
    pxor            m3, m15
    pxor            m4, m15
    movu  [px+2*64-%1], m1
    movu  [px+3*64-%1], m2
    movu  [px+4*64-%1], m3
    movu  [px+5*64-%1], m4
    pmovzxbw        m1, [bufq+buf_stride*6-(%1/2)]
    pmovzxbw        m2, [bufq+buf_stride*7-(%1/2)]
    pmovzxbw        m3, [bufq+buf_stride*8-(%1/2)]
    pmovzxbw        m4, [bufq+buf_stride*9-(%1/2)]
    pxor            m1, m15
    pxor            m2, m15
    pxor            m3, m15
    pxor            m4, m15
    movu  [px+6*64-%1], m1
    movu  [px+7*64-%1], m2
    movu  [px+8*64-%1], m3
    movu  [px+9*64-%1], m4
 %endif
%endif

    test         edgeb, 2                   ; have_right
    jnz .body_done

    ; no right
%if %3 == 1
    movq [px+0*64+%1*4], xm14
    movq [px+1*64+%1*4], xm14
    movq [px+2*64+%1*4], xm14
    movq [px+3*64+%1*4], xm14
%else
    movd [px+0*64+%1*2], xm14
    movd [px+1*64+%1*2], xm14
    movd [px+2*64+%1*2], xm14
    movd [px+3*64+%1*2], xm14
%endif
%if %2 == 8
 %if %3 == 1
    movq [px+4*64+%1*4], xm14
    movq [px+5*64+%1*4], xm14
    movq [px+6*64+%1*4], xm14
    movq [px+7*64+%1*4], xm14
 %else
    movd [px+4*64+%1*2], xm14
    movd [px+5*64+%1*2], xm14
    movd [px+6*64+%1*2], xm14
    movd [px+7*64+%1*2], xm14
 %endif
%endif
.body_done:

    ; top
    test         edgeb, 4                    ; have_top
    jz .no_top
    test         edgeb, 1                    ; have_left
    jz .top_no_left
    test         edgeb, 2                    ; have_right
    jnz .top_done
    ; top no right
%if %3 == 1
    movq [px-2*64+%1*4], xm14
    movq [px-1*64+%1*4], xm14
%else
    movd [px-2*64+%1*2], xm14
    movd [px-1*64+%1*2], xm14
%endif
    jmp .top_done
.top_no_left:
    test         edgeb, 2                   ; have_right
    jz .top_no_left_right
%if %3 == 1
    movq   [px-2*64-8], xm14
    movq   [px-1*64-8], xm14
%else
    movd   [px-2*64-4], xm14
    movd   [px-1*64-4], xm14
%endif
    jmp .top_done
.top_no_left_right:
%if %3 == 1
    movq    [px-2*64-8], xm14
    movq    [px-1*64-8], xm14
    movq [px-2*64+%1*4], xm14
    movq [px-1*64+%1*4], xm14
%else
    movd    [px-2*64-4], xm14
    movd    [px-1*64-4], xm14
    movd [px-2*64+%1*2], xm14
    movd [px-1*64+%1*2], xm14
%endif
    jmp .top_done
.no_top:
%if %3 == 1
    movu [px-2*64-8], m14
    movu [px-1*64-8], m14
%if %1 == 8
    movu [px-2*64-8+32], xm14
    movu [px-1*64-8+32], xm14
%endif
%else
    movu   [px-2*64-%1], m14
    movu   [px-1*64-%1], m14
%endif
.top_done:

    ; left
    test         edgeb, 1                   ; have_left
    jnz .left_done
.no_left:
%if %3 == 1
    movq   [px+0*64-8], xm14
    movq   [px+1*64-8], xm14
    movq   [px+2*64-8], xm14
    movq   [px+3*64-8], xm14
%else
    movd   [px+0*64-4], xm14
    movd   [px+1*64-4], xm14
    movd   [px+2*64-4], xm14
    movd   [px+3*64-4], xm14
%endif
%if %2 == 8
%if %3 == 1
    movq   [px+4*64-8], xm14
    movq   [px+5*64-8], xm14
    movq   [px+6*64-8], xm14
    movq   [px+7*64-8], xm14
%else
    movd   [px+4*64-4], xm14
    movd   [px+5*64-4], xm14
    movd   [px+6*64-4], xm14
    movd   [px+7*64-4], xm14
%endif
%endif
.left_done:

    ; bottom
    test         edgeb, 8                   ; have_bottom
    jz .no_bottom
    test         edgeb, 1                   ; have_left
    jz .bottom_no_left
    test         edgeb, 2                   ; have_right
    jnz .bottom_done
    ; bottom_no_right
%if %3 == 1
    movq  [px+(%2+0)*64+%1*4], xm14
    movq  [px+(%2+1)*64+%1*4], xm14
%else
    movd  [px+(%2+0)*64+%1*2], xm14
    movd  [px+(%2+1)*64+%1*2], xm14
%endif
    jmp .bottom_done
.bottom_no_left:
    test          edgeb, 2                  ; have_right
    jz .bottom_no_left_right
%if %3 == 1
    movq   [px+(%2+0)*64-8], xm14
    movq   [px+(%2+1)*64-8], xm14
%else
    movd   [px+(%2+0)*64-4], xm14
    movd   [px+(%2+1)*64-4], xm14
%endif
    jmp .bottom_done
.bottom_no_left_right:
%if %3 == 1
    movq    [px+(%2+0)*64-8], xm14
    movq    [px+(%2+1)*64-8], xm14
    movq [px+(%2+0)*64+%1*4], xm14
    movq [px+(%2+1)*64+%1*4], xm14
%else
    movd    [px+(%2+0)*64-4], xm14
    movd    [px+(%2+1)*64-4], xm14
    movd [px+(%2+0)*64+%1*2], xm14
    movd [px+(%2+1)*64+%1*2], xm14
%endif
    jmp .bottom_done
.no_bottom:
%if %3 == 1
    movu [px+(%2+0)*64-8], m14
    movu [px+(%2+1)*64-8], m14
%if %1 == 8
    movu [px+(%2+0)*64-8+32], xm14
    movu [px+(%2+1)*64-8+32], xm14
%endif
%else
    movu   [px+(%2+0)*64-%1], m14
    movu   [px+(%2+1)*64-%1], m14
%endif
.bottom_done:

    ; actual filter
 DEFINE_ARGS dst, stride, pridmp, zero, pri, secdmp, dir, damping
%undef edged
    ; register to shuffle values into after packing
    vbroadcasti128 m12, [shufb_lohi]

    movifnidn dampingd, dampingm
    xor          zerod, zerod
    sub       dampingd, 31
    test          prid, prid
    jz .border_sec_only
    movd           xm0, prid
    lzcnt      pridmpd, prid
    add        pridmpd, dampingd
    cmovs      pridmpd, zerod
    mov        [rsp+0], pridmpq                 ; pri_shift
    test       secdmpd, secdmpd
    jz .border_pri_only
    movd           xm1, secdmpd
    lzcnt      secdmpd, secdmpd
    add        secdmpd, dampingd
    mov        [rsp+8], secdmpq                 ; sec_shift

 DEFINE_ARGS dst, stride, pridmp, zero, pri, secdmp, dir, table
    lea         tableq, [tap_table]
    vpbroadcastd   m13, [tableq+pridmpq*4]      ; pri_shift_mask
    vpbroadcastd   m14, [tableq+secdmpq*4]      ; sec_shift_mask

    ; pri/sec_taps[k] [4 total]
 DEFINE_ARGS dst, stride, _, _, tap, _, dir, table
    vpbroadcastb    m0, xm0                     ; pri_strength
    vpbroadcastb    m1, xm1                     ; sec_strength
    and           tapd, 1
    ; pri_taps/sec_taps
    lea           tapq, [tableq+tapq*4+tap_weights-tap_table]

    BORDER_PREP_REGS %1, %2, %3
.border_v_loop:
    BORDER_LOAD_BLOCK %1, %2, %3, 1
.border_k_loop:
    vpbroadcastb    m2, [tapq+kq]               ; pri_taps
    vpbroadcastb    m3, [tapq+kq+2]             ; sec_taps
    ACCUMULATE_TAP_WORD 0*2, [rsp+0], m13, m0, m2, %1, %2, %3, 1
    ACCUMULATE_TAP_WORD 2*2, [rsp+8], m14, m1, m3, %1, %2, %3, 1
    ACCUMULATE_TAP_WORD 6*2, [rsp+8], m14, m1, m3, %1, %2, %3, 1
    dec             kq
    jge .border_k_loop

    vpbroadcastd   m10, [pw_2048]
%if %3 == 1
    vbroadcasti128  m2, [pb_deinterleave]
%endif
    BORDER_ADJUST_PIXEL %1, %2, %3, m10, xm2, 1
%if %3 == 1
 %define vloop_lines (mmsize/(%1*4))
    lea          dst1q, [dst1q+strideq*vloop_lines]
    lea          dst2q, [dst2q+strideq*vloop_lines]
%else
 %define vloop_lines (mmsize/(%1*2))
    lea           dstq, [dstq+strideq*vloop_lines]
%endif
    add           stkq, 64*vloop_lines
    dec             hd
    jg .border_v_loop
    RET

.border_pri_only:
 DEFINE_ARGS dst, stride, pridmp, _, pri, _, dir, table
    lea         tableq, [tap_table]
    vpbroadcastd   m13, [tableq+pridmpq*4]      ; pri_shift_mask
 DEFINE_ARGS dst, stride, pridmp, _, tap, _, dir, table
    vpbroadcastb    m0, xm0                     ; pri_strength
    and           tapd, 1
    ; pri_taps
    lea           tapq, [tableq+tapq*4+tap_weights-tap_table]
    BORDER_PREP_REGS %1, %2, %3
    vpbroadcastd    m1, [pw_2048]
%if %3 == 1
    vbroadcasti128  m3, [pb_deinterleave]
%endif
.border_pri_v_loop:
    BORDER_LOAD_BLOCK %1, %2, %3
.border_pri_k_loop:
    vpbroadcastb    m2, [tapq+kq]               ; pri_taps
    ACCUMULATE_TAP_WORD 0*2, [rsp+0], m13, m0, m2, %1, %2, %3
    dec             kq
    jge .border_pri_k_loop
    BORDER_ADJUST_PIXEL %1, %2, %3, m1, xm3
%if %3 == 1
 %define vloop_lines (mmsize/(%1*4))
    lea          dst1q, [dst1q+strideq*vloop_lines]
    lea          dst2q, [dst2q+strideq*vloop_lines]
%else
 %define vloop_lines (mmsize/(%1*2))
    lea           dstq, [dstq+strideq*vloop_lines]
%endif
    add           stkq, 64*vloop_lines
    dec             hd
    jg .border_pri_v_loop
    RET

.border_sec_only:
 DEFINE_ARGS dst, stride, _, _, _, secdmp, dir, damping
    movd           xm1, secdmpd
    lzcnt      secdmpd, secdmpd
    add        secdmpd, dampingd
    mov        [rsp+8], secdmpq                 ; sec_shift
 DEFINE_ARGS dst, stride, _, _, _, secdmp, dir, table
    lea         tableq, [tap_table]
    vpbroadcastd   m14, [tableq+secdmpq*4]      ; sec_shift_mask
 DEFINE_ARGS dst, stride, _, _, tap, _, dir, table
    vpbroadcastb    m1, xm1                     ; sec_strength
    ; sec_taps
    lea           tapq, [tableq+2+tap_weights-tap_table]
    BORDER_PREP_REGS %1, %2, %3
    vpbroadcastd    m0, [pw_2048]
%if %3 == 1
    vbroadcasti128  m2, [pb_deinterleave]
%endif
.border_sec_v_loop:
    BORDER_LOAD_BLOCK %1, %2, %3
.border_sec_k_loop:
    vpbroadcastb    m3, [tapq+kq]               ; sec_taps
    ACCUMULATE_TAP_WORD 2*2, [rsp+8], m14, m1, m3, %1, %2, %3
    ACCUMULATE_TAP_WORD 6*2, [rsp+8], m14, m1, m3, %1, %2, %3
    dec             kq
    jge .border_sec_k_loop
    BORDER_ADJUST_PIXEL %1, %2, %3, m0, xm2
%if %3 == 1
 %define vloop_lines (mmsize/(%1*4))
    lea          dst1q, [dst1q+strideq*vloop_lines]
    lea          dst2q, [dst2q+strideq*vloop_lines]
%else
 %define vloop_lines (mmsize/(%1*2))
    lea           dstq, [dstq+strideq*vloop_lines]
%endif
    add           stkq, 64*vloop_lines
    dec             hd
    jg .border_sec_v_loop
    RET
%endmacro

INIT_YMM avx2
cglobal cdef_prep_y_8bpc, 7, 9, 5, buf, src, stride, top, bot, num_blocks, \
                                   edge, edgejmp, table
%define buf_stride CDEF_BUFFER_Y_STRIDE
; Start of the first complete block in buf.
%define bufp (bufq+8)
    vpbroadcastd    m4, [pb_128]
    ; If less than 8 cdef blocks left in the frame, copy block by block
    test   num_blocksd, 8
    jz .slow_path
    mov          edged, edged
    lea         tableq, [cdef_prep_y_8bpc_jmptable]
    movsxd    edgejmpq, [tableq+edgeq*4]
    add       edgejmpq, tableq

    test         edgeb, 4
    jz .skip_top

    call edgejmpq
.skip_top:
    add           bufq, buf_stride*2

    mov           topq, srcq
 DEFINE_ARGS buf, h, stride, src, bot, num_blocks, edge, edgejmp
    mov             hd, 4
.loop:
    call edgejmpq
    add           bufq, buf_stride*2
    lea           srcq, [srcq+strideq*2]
    dec             hd
    jg .loop

    test         edgeb, 8
    jz .skip_bottom

    mov           srcq, botq
    call edgejmpq
.skip_bottom:
    RET

 DEFINE_ARGS buf, _, stride, src, bot, num_blocks, edge, edgejmp
.left_skip_full:
    movd           xm0, [srcq        -4]
    movd           xm1, [srcq+strideq-4]
    pxor           xm0, xm4
    pxor           xm1, xm4
    movd [bufp           -4], xm0
    movd [bufp+buf_stride-4], xm1
    pxor            m0, m4, [srcq           ]
    pxor            m1, m4, [srcq        +32]
    pxor            m2, m4, [srcq+strideq   ]
    pxor            m3, m4, [srcq+strideq+32]
    movu [bufp              ], m0
    movu [bufp           +32], m1
    movu [bufp+buf_stride   ], m2
    movu [bufp+buf_stride+32], m3
    movd           xm0, [srcq        +64]
    movd           xm1, [srcq+strideq+64]
    pxor           xm0, xm4
    pxor           xm1, xm4
    movd [bufp           +64], xm0
    movd [bufp+buf_stride+64], xm1
    ret
.full:
    movd           xm0, [bufp           +60]
    movd           xm1, [bufp+buf_stride+60]
    movd [bufp           -4], xm0
    movd [bufp+buf_stride-4], xm1
.no_left:
    pxor            m0, m4, [srcq           ]
    pxor            m1, m4, [srcq        +32]
    pxor            m2, m4, [srcq+strideq   ]
    pxor            m3, m4, [srcq+strideq+32]
    movu [bufp              ], m0
    movu [bufp           +32], m1
    movu [bufp+buf_stride   ], m2
    movu [bufp+buf_stride+32], m3
    movd           xm0, [srcq        +64]
    movd           xm1, [srcq+strideq+64]
    pxor           xm0, xm4
    pxor           xm1, xm4
    movd [bufp           +64], xm0
    movd [bufp+buf_stride+64], xm1
    ret

.left_skip_no_right:
    movd           xm0, [srcq        -4]
    movd           xm1, [srcq+strideq-4]
    pxor           xm0, xm4
    pxor           xm1, xm4
    movd [bufp           -4], xm0
    movd [bufp+buf_stride-4], xm1
    jmp .no_left_right
.no_right:
    movd           xm0, [bufp           +60]
    movd           xm1, [bufp+buf_stride+60]
    movd [bufp           -4], xm0
    movd [bufp+buf_stride-4], xm1
.no_left_right:
    pxor            m0, m4, [srcq           ]
    pxor            m1, m4, [srcq        +32]
    pxor            m2, m4, [srcq+strideq   ]
    pxor            m3, m4, [srcq+strideq+32]
    movu [bufp              ], m0
    movu [bufp           +32], m1
    movu [bufp+buf_stride   ], m2
    movu [bufp+buf_stride+32], m3
    ret

.slow_path:
    PUSH r9
DEFINE_ARGS buf, src, stride, top, bot, num_blocks, edge, w, src_backup, \
            buf_backup
    test         edgeb, 4
    jz .slow_path_skip_top
    call .slow_path_row
    jmp .slow_path_top_done
.slow_path_skip_top:
    add           bufq, buf_stride*2
.slow_path_top_done:
    mov           topq, srcq
    call .slow_path_row
    call .slow_path_row
    call .slow_path_row
    call .slow_path_row

    test         edgeb, 8
    jz .slow_path_skip_bottom

    mov           topq, botq
    call .slow_path_row
.slow_path_skip_bottom:
    POP r9
    RET

.slow_path_row:
    DEFINE_ARGS buf, _, stride, src, bot, num_blocks, edge, w, src_backup, \
                buf_backup
    mov    src_backupq, srcq
    mov    buf_backupq, bufq
    mov             wd, num_blocksd
    test         edgeb, 1
    jz .slow_path_horz_loop
    test         edgeb, 16
    jnz .slow_path_left_skip
    movd           xm0, [bufp           +60]
    movd           xm1, [bufp+buf_stride+60]

    movd [bufp           -4], xm0
    movd [bufp+buf_stride-4], xm1
    jmp .slow_path_horz_loop
.slow_path_left_skip:
    movd           xm0, [srcq        -4]
    movd           xm1, [srcq+strideq-4]
    pxor           xm0, xm4
    pxor           xm1, xm4
    movd [bufp           -4], xm0
    movd [bufp+buf_stride-4], xm1

.slow_path_horz_loop:
    movq           xm0, [srcq]
    movq           xm1, [srcq+strideq]
    pxor           xm0, xm4
    pxor           xm1, xm4
    movq [bufp           ], xm0
    movq [bufp+buf_stride], xm1
    add           srcq, 8
    add           bufq, 8
    dec             wd
    jge .slow_path_horz_loop
    lea           srcq, [src_backupq+strideq*2]
    lea           bufq, [buf_backupq+buf_stride*2]
    ret

%macro CDEF_PREP_UV 3 ; w, h, chroma subsampling name
INIT_YMM avx2
cglobal cdef_prep_uv_%3_8bpc, 7, 9, 7, buf, src, stride, top, bot, \
                                       num_blocks, edge, edgejmp, table
%define buf_stride (CDEF_BUFFER_Y_STRIDE * 2 * %1 / 8)
; Start of the first complete block in buf.
%define bufp (bufq+8)
    vpbroadcastd    m6, [pb_128]
    ; If less than 8 cdef blocks left in the frame, copy block by block
    test   num_blocksd, 8
    jz .slow_path
    mov          edged, edged
    lea         tableq, [cdef_prep_uv_%3_8bpc_jmptable]
    movsxd    edgejmpq, [tableq+edgeq*4]
    add       edgejmpq, tableq
    test         edgeb, 4
    jz .skip_top

 DEFINE_ARGS buf, src, stride, top1, bot, num_blocks, edge, edgejmp, top2
    mov          top2q, [top1q+gprsize]
    mov          top1q, [top1q]
    call edgejmpq
.skip_top:
    add           bufq, buf_stride*2

%if %2 == 4
 DEFINE_ARGS buf, src, stride, src1, bot, num_blocks, edge, edgejmp, src2
    mov          src1q, [srcq]
    mov          src2q, [srcq+gprsize]
    call edgejmpq
    add           bufq, buf_stride*2
    lea          src1q, [src1q+strideq*2]
    lea          src2q, [src2q+strideq*2]
    call edgejmpq
    add           bufq, buf_stride*2
%else
 DEFINE_ARGS buf, src, stride, src1, bot, num_blocks, edge, edgejmp, src2
    mov          src1q, [srcq]
    mov          src2q, [srcq+gprsize]
 DEFINE_ARGS buf, h, stride, src1, bot, num_blocks, edge, edgejmp, src2
    mov             hd, %2 / 2
.loop:
    call edgejmpq
    add           bufq, buf_stride*2
    lea          src1q, [src1q+strideq*2]
    lea          src2q, [src2q+strideq*2]
    dec             hd
    jg .loop
%endif

    test         edgeb, 8
    jz .skip_bottom

    mov          src1q, [botq        ]
    mov          src2q, [botq+gprsize]
    call edgejmpq
.skip_bottom:
    RET

%macro LEFT 2
    movd           xm0, [bufp           +16*%1-4]
    movd           xm1, [bufp+buf_stride+16*%1-4]
    movd [bufp           -4], xm0
    movd [bufp+buf_stride-4], xm1
%endmacro

%macro LEFT_SKIP 2
    movd           xm0, [src1q        -4]
    movd           xm1, [src2q        -4]
    movd           xm2, [src1q+strideq-4]
    movd           xm3, [src2q+strideq-4]
    punpcklbw      xm0, xm1
    punpcklbw      xm2, xm3
    pxor           xm0, xm6
    pxor           xm2, xm6
    movq [bufp           -8], xm0
    movq [bufp+buf_stride-8], xm2
%endmacro

%macro MIDDLE 2
    pxor            m0, m6, [src1q        ]
    pxor            m1, m6, [src2q        ]
    pxor            m2, m6, [src1q+strideq]
    pxor            m3, m6, [src2q+strideq]
    punpckhbw       m4, m0, m1 ; columns: [8,15], [24-31]
    punpcklbw       m0, m1     ; columns: [0,7],  [16,23]
    punpckhbw       m5, m2, m3
    punpcklbw       m2, m3
    vperm2i128      m1, m0, m4, 0x31
    vinserti128     m0, xm4, 1
    vperm2i128      m3, m2, m5, 0x31
    vinserti128     m2, xm5, 1
    movu [bufp              ], m0
    movu [bufp           +32], m1
    movu [bufp+buf_stride   ], m2
    movu [bufp+buf_stride+32], m3
%if %1 == 8
    pxor            m0, m6, [src1q        +32]
    pxor            m1, m6, [src2q        +32]
    pxor            m2, m6, [src1q+strideq+32]
    pxor            m3, m6, [src2q+strideq+32]
    punpckhbw       m4, m0, m1 ; columns: [8,15], [24-31]
    punpcklbw       m0, m1     ; columns: [0,7],  [16,23]
    punpckhbw       m5, m2, m3
    punpcklbw       m2, m3
    vperm2i128      m1, m0, m4, 0x31
    vinserti128     m0, xm4, 1
    vperm2i128      m3, m2, m5, 0x31
    vinserti128     m2, xm5, 1
    movu [bufp           +64], m0
    movu [bufp           +96], m1
    movu [bufp+buf_stride+64], m2
    movu [bufp+buf_stride+96], m3
%endif
%endmacro

%macro RIGHT 2
    movd           xm0, [src1q        +8*%1]
    movd           xm1, [src2q        +8*%1]
    movd           xm2, [src1q+strideq+8*%1]
    movd           xm3, [src2q+strideq+8*%1]
    punpcklbw      xm0, xm1
    punpcklbw      xm2, xm3
    pxor           xm0, xm6
    pxor           xm2, xm6
    movd [bufp           +16*%1], xm0
    movd [bufp+buf_stride+16*%1], xm2
%endmacro

; Share 420 routines with 422
%if !(%1 == 4 && %2 == 8)
 DEFINE_ARGS buf, _, stride, src1, bot, num_blocks, edge, edgejmp, src2

.left_skip_full:
LEFT_SKIP %1, %2
MIDDLE %1, %2
RIGHT %1, %2
    ret

.full:
LEFT %1, %2
.no_left:
MIDDLE %1, %2
RIGHT %1, %2
    ret

.left_skip_no_right:
LEFT_SKIP %1, %2
    jmp .no_left_right
.no_right:
LEFT %1, %2
.no_left_right:
MIDDLE %1, %2
    ret
%endif

.slow_path:
    PUSH r9
    PUSH r10
    PUSH r11
DEFINE_ARGS buf, src, stride, top1, bot, num_blocks, edge, w, top2, \
            src1_backup, src2_backup, buf_backup
    test         edgeb, 4
    jz .slow_path_skip_top
    mov          top2q, [top1q+gprsize]
    mov          top1q, [top1q]
DEFINE_ARGS buf, src, stride, src1, bot, num_blocks, edge, w, src2, \
            src1_backup, src2_backup, buf_backup
    call .slow_path_row
    jmp .slow_path_top_done
.slow_path_skip_top:
    add           bufq, buf_stride*2
.slow_path_top_done:
    mov          src1q, [srcq]
    mov          src2q, [srcq+gprsize]

    call .slow_path_row
    call .slow_path_row
%if %2 == 8
    call .slow_path_row
    call .slow_path_row
%endif

    test         edgeb, 8
    jz .slow_path_skip_bottom

    mov          src1q, [botq]
    mov          src2q, [botq+gprsize]
    call .slow_path_row
.slow_path_skip_bottom:
    POP r11
    POP r10
    POP r9
    RET

.slow_path_row:
    DEFINE_ARGS buf, _, stride, src1, bot, num_blocks, edge, w, src2, \
                src1_backup, src2_backup, buf_backup
    mov   src1_backupq, src1q
    mov   src2_backupq, src2q
    mov    buf_backupq, bufq
    mov             wd, num_blocksd
    test         edgeb, 1
    jz .slow_path_horz_loop
    test         edgeb, 16
    jnz .slow_path_left_skip
    LEFT %1, %2
    jmp .slow_path_horz_loop
.slow_path_left_skip:
    LEFT_SKIP %1, %2

.slow_path_horz_loop:
%if %1 == 4
    movd           xm0, [src1q        ]
    movd           xm1, [src2q        ]
    movd           xm2, [src1q+strideq]
    movd           xm3, [src2q+strideq]
    punpcklbw      xm0, xm1
    punpcklbw      xm2, xm3
    pxor           xm0, xm6
    pxor           xm2, xm6
    movq [bufp             ], xm0
    movq [bufp+buf_stride*1], xm2
%else
    movq           xm0, [src1q        ]
    movq           xm1, [src2q        ]
    movq           xm2, [src1q+strideq]
    movq           xm3, [src2q+strideq]
    punpcklbw      xm0, xm1
    punpcklbw      xm2, xm3
    pxor           xm0, xm6
    pxor           xm2, xm6
    movu [bufp             ], xm0
    movu [bufp+buf_stride*1], xm2
%endif
    add          src1q, %1
    add          src2q, %1
    add           bufq, %1*2
    dec             wd
    jge .slow_path_horz_loop
    lea          src1q, [src1_backupq+strideq*2]
    lea          src2q, [src2_backupq+strideq*2]
    lea           bufq, [buf_backupq+buf_stride*2]
    ret

%unmacro LEFT 2
%unmacro LEFT_SKIP 2
%unmacro MIDDLE 2
%unmacro RIGHT 2
%endmacro

CDEF_FILTER 8, 8, 0
CDEF_PREP_UV 8, 8, 444
CDEF_FILTER 8, 8, 1
CDEF_PREP_UV 4, 8, 422
CDEF_FILTER 4, 8, 1
CDEF_PREP_UV 4, 4, 420
CDEF_FILTER 4, 4, 1

INIT_YMM avx2
cglobal cdef_dir_8bpc, 3, 4, 6, src, stride, var, stride3
    lea       stride3q, [strideq*3]
    movq           xm0, [srcq+strideq*0]
    movq           xm1, [srcq+strideq*1]
    movq           xm2, [srcq+strideq*2]
    movq           xm3, [srcq+stride3q ]
    lea           srcq, [srcq+strideq*4]
    vpbroadcastq    m4, [srcq+stride3q ]
    vpbroadcastq    m5, [srcq+strideq*2]
    vpblendd        m0, m4, 0xf0
    vpblendd        m1, m5, 0xf0
    vpbroadcastq    m4, [srcq+strideq*1]
    vpbroadcastq    m5, [srcq+strideq*0]
    vpblendd        m2, m4, 0xf0
    vpblendd        m3, m5, 0xf0
    pxor            m4, m4
    punpcklbw       m0, m4
    punpcklbw       m1, m4
    punpcklbw       m2, m4
    punpcklbw       m3, m4
cglobal_label .main
    vpbroadcastd    m4, [pw_128]
    PROLOGUE 3, 4, 15
    psubw           m0, m4
    psubw           m1, m4
    psubw           m2, m4
    psubw           m3, m4

    ; shuffle registers to generate partial_sum_diag[0-1] together
    vperm2i128      m7, m0, m0, 0x01
    vperm2i128      m6, m1, m1, 0x01
    vperm2i128      m5, m2, m2, 0x01
    vperm2i128      m4, m3, m3, 0x01

    ; start with partial_sum_hv[0-1]
    paddw           m8, m0, m1
    paddw           m9, m2, m3
    phaddw         m10, m0, m1
    phaddw         m11, m2, m3
    paddw           m8, m9
    phaddw         m10, m11
    vextracti128   xm9, m8, 1
    vextracti128  xm11, m10, 1
    paddw          xm8, xm9                 ; partial_sum_hv[1]
    phaddw        xm10, xm11                ; partial_sum_hv[0]
    vinserti128     m8, xm10, 1
    vpbroadcastd    m9, [div_table+44]
    pmaddwd         m8, m8
    pmulld          m8, m9                  ; cost6[2a-d] | cost2[a-d]

    ; create aggregates [lower half]:
    ; m9 = m0:01234567+m1:x0123456+m2:xx012345+m3:xxx01234+
    ;      m4:xxxx0123+m5:xxxxx012+m6:xxxxxx01+m7:xxxxxxx0
    ; m10=             m1:7xxxxxxx+m2:67xxxxxx+m3:567xxxxx+
    ;      m4:4567xxxx+m5:34567xxx+m6:234567xx+m7:1234567x
    ; and [upper half]:
    ; m9 = m0:xxxxxxx0+m1:xxxxxx01+m2:xxxxx012+m3:xxxx0123+
    ;      m4:xxx01234+m5:xx012345+m6:x0123456+m7:01234567
    ; m10= m0:1234567x+m1:234567xx+m2:34567xxx+m3:4567xxxx+
    ;      m4:567xxxxx+m5:67xxxxxx+m6:7xxxxxxx
    ; and then shuffle m11 [shufw_6543210x], unpcklwd, pmaddwd, pmulld, paddd

    pslldq          m9, m1, 2
    psrldq         m10, m1, 14
    pslldq         m11, m2, 4
    psrldq         m12, m2, 12
    pslldq         m13, m3, 6
    psrldq         m14, m3, 10
    paddw           m9, m11
    paddw          m10, m12
    paddw           m9, m13
    paddw          m10, m14
    pslldq         m11, m4, 8
    psrldq         m12, m4, 8
    pslldq         m13, m5, 10
    psrldq         m14, m5, 6
    paddw           m9, m11
    paddw          m10, m12
    paddw           m9, m13
    paddw          m10, m14
    pslldq         m11, m6, 12
    psrldq         m12, m6, 4
    pslldq         m13, m7, 14
    psrldq         m14, m7, 2
    paddw           m9, m11
    paddw          m10, m12
    paddw           m9, m13
    paddw          m10, m14                 ; partial_sum_diag[0/1][8-14,zero]
    vbroadcasti128 m14, [shufw_6543210x]
    vbroadcasti128 m13, [div_table+16]
    vbroadcasti128 m12, [div_table+0]
    paddw           m9, m0                  ; partial_sum_diag[0/1][0-7]
    pshufb         m10, m14
    punpckhwd      m11, m9, m10
    punpcklwd       m9, m10
    pmaddwd        m11, m11
    pmaddwd         m9, m9
    pmulld         m11, m13
    pmulld          m9, m12
    paddd           m9, m11                 ; cost0[a-d] | cost4[a-d]

    ; merge horizontally and vertically for partial_sum_alt[0-3]
    paddw          m10, m0, m1
    paddw          m11, m2, m3
    paddw          m12, m4, m5
    paddw          m13, m6, m7
    phaddw          m0, m4
    phaddw          m1, m5
    phaddw          m2, m6
    phaddw          m3, m7

    ; create aggregates [lower half]:
    ; m4 = m10:01234567+m11:x0123456+m12:xx012345+m13:xxx01234
    ; m11=              m11:7xxxxxxx+m12:67xxxxxx+m13:567xxxxx
    ; and [upper half]:
    ; m4 = m10:xxx01234+m11:xx012345+m12:x0123456+m13:01234567
    ; m11= m10:567xxxxx+m11:67xxxxxx+m12:7xxxxxxx
    ; and then pshuflw m11 3012, unpcklwd, pmaddwd, pmulld, paddd

    pslldq          m4, m11, 2
    psrldq         m11, 14
    pslldq          m5, m12, 4
    psrldq         m12, 12
    pslldq          m6, m13, 6
    psrldq         m13, 10
    paddw           m4, m10
    paddw          m11, m12
    vpbroadcastd   m12, [div_table+44]
    paddw           m5, m6
    paddw          m11, m13                 ; partial_sum_alt[3/2] right
    vbroadcasti128 m13, [div_table+32]
    paddw           m4, m5                  ; partial_sum_alt[3/2] left
    pshuflw         m5, m11, q3012
    punpckhwd       m6, m11, m4
    punpcklwd       m4, m5
    pmaddwd         m6, m6
    pmaddwd         m4, m4
    pmulld          m6, m12
    pmulld          m4, m13
    paddd           m4, m6                  ; cost7[a-d] | cost5[a-d]

    ; create aggregates [lower half]:
    ; m5 = m0:01234567+m1:x0123456+m2:xx012345+m3:xxx01234
    ; m1 =             m1:7xxxxxxx+m2:67xxxxxx+m3:567xxxxx
    ; and [upper half]:
    ; m5 = m0:xxx01234+m1:xx012345+m2:x0123456+m3:01234567
    ; m1 = m0:567xxxxx+m1:67xxxxxx+m2:7xxxxxxx
    ; and then pshuflw m1 3012, unpcklwd, pmaddwd, pmulld, paddd

    pslldq          m5, m1, 2
    psrldq          m1, 14
    pslldq          m6, m2, 4
    psrldq          m2, 12
    pslldq          m7, m3, 6
    psrldq          m3, 10
    paddw           m5, m0
    paddw           m1, m2
    paddw           m6, m7
    paddw           m1, m3                  ; partial_sum_alt[0/1] right
    paddw           m5, m6                  ; partial_sum_alt[0/1] left
    pshuflw         m0, m1, q3012
    punpckhwd       m1, m5
    punpcklwd       m5, m0
    pmaddwd         m1, m1
    pmaddwd         m5, m5
    pmulld          m1, m12
    pmulld          m5, m13
    paddd           m5, m1                  ; cost1[a-d] | cost3[a-d]

    mova           xm0, [pd_47130256+ 16]
    mova            m1, [pd_47130256]
    phaddd          m9, m8
    phaddd          m5, m4
    phaddd          m9, m5
    vpermd          m0, m9                  ; cost[0-3]
    vpermd          m1, m9                  ; cost[4-7] | cost[0-3]

    ; now find the best cost
    pmaxsd         xm2, xm0, xm1
    pshufd         xm3, xm2, q1032
    pmaxsd         xm2, xm3
    pshufd         xm3, xm2, q2301
    pmaxsd         xm2, xm3 ; best cost

    ; find the idx using minpos
    ; make everything other than the best cost negative via subtraction
    ; find the min of unsigned 16-bit ints to sort out the negative values
    psubd          xm4, xm1, xm2
    psubd          xm3, xm0, xm2
    packssdw       xm3, xm4
    phminposuw     xm3, xm3

    ; convert idx to 32-bits
    psrld          xm3, 16
    movd           eax, xm3

    ; get idx^4 complement
    vpermd          m3, m1
    psubd          xm2, xm3
    psrld          xm2, 10
    movd        [varq], xm2
    RET

%endif ; ARCH_X86_64
