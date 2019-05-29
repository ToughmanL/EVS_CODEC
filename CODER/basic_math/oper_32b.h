/* Double precision operations */
/* $Id: oper_32b.h 1094 2014-02-10 17:12:11Z jdr $ */
#ifndef _OPER_32b_H
#define _OPER_32b_H

void L_Extract (Word32 L_32, Word16 *hi, Word16 *lo);
Word16 L_Extract_lc (Word32 L_32, Word16 *hi);
Word32 L_Comp (Word16 hi, Word16 lo);
Word32 Mpy_32 (Word16 hi1, Word16 lo1, Word16 hi2, Word16 lo2);
Word32 Mac_32 (Word32 L_num, Word16 hi1, Word16 lo1, Word16 hi2, Word16 lo2);
Word32 Sqr_32 (Word16 hi, Word16 lo);
Word32 Sad_32 (Word32 L_num, Word16 hi, Word16 lo);
Word32 Mpy_32_16 (Word16 hi, Word16 lo, Word16 n);
Word32 Mac_32_16 (Word32 L_num, Word16 hi, Word16 lo, Word16 n);
Word32 Msu_32_16 (Word32 L_num, Word16 hi, Word16 lo, Word16 n);
Word32 Div_32 (Word32 L_num, Word16 denom_hi, Word16 denom_lo);

#endif
