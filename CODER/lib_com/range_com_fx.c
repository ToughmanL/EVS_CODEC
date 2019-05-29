/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/

#include <stdlib.h>
#include "options.h"    /* Compilation switches                   */
#include "cnst_fx.h"    /* Common constants                       */
#include "prot_fx.h"    /* Function prototypes                    */
#include "stl.h"        /* required for wmc_tool */


/*-------------------------------------------------------------------*
 * rc_get_bits2()
 *
 *  Get number of bits needed to finalize range coder
 *-------------------------------------------------------------------*/

Word16 rc_get_bits2_fx(             /* o: Number of bits needed         */
    const Word16 N,                 /* i: Number of bits currently used */
    const UWord32 range             /* i: Range of range coder          */
)
{
    return add(add(N, 2), norm_ul(range));
}

void rangeCoderFinalizationFBits_fx(
    Word16 Brc,
    UWord32 INTrc,
    Word16 *FBits
)
{
    Word32 L_Bq15;
    UWord32 h, UL_tmp;
    UWord16 Bq15ui16, l;
    Word16 B, E, x, k;
    *FBits = shl(add(Brc, 32), 3);

    B = sub(30, norm_ul(INTrc));
    x = sub(B, RCF_INIT_SHIFT );
    L_Bq15 = 0;
    move16();
    if (x >= 0)
    {
        L_Bq15 = (Word32)UL_lshr(INTrc, x);
    }

    E = 2;
    move16();
    FOR(k = 1; k < 4; k++)
    {
        Bq15ui16 = u_extract_l(L_shr(L_Bq15, s_and(E, 1)));
        UL_tmp   = UL_lshl(UL_deposit_l(Bq15ui16), 1);
        Mpy_32_16_uu(UL_tmp, Bq15ui16, &h , &l);
        L_Bq15   = (Word32) h;
        E        = add(shl(B, 1), extract_l(L_lshr(L_sub(((1L << 16) - 1L), L_Bq15), 31)));
        B        = E;
        move16();
    }
    *FBits  = sub(*FBits, B);
    return;
}

