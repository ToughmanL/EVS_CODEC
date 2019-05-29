/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/

#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include "cnst_fx.h"
#include "prot_fx.h"
#include "basop_util.h"
#include "stl.h"

static Word16 TCX_MDCT_GetScaleFactor(Word16 L, Word16 *factor_e)
{

    Word16 factor;

    IF(sub(L, NORM_MDCT_FACTOR) == 0)
    {
        factor = 32767;
        move16();
        *factor_e = 0;
        move16();
    }
    ELSE IF(sub(L, 2*NORM_MDCT_FACTOR) == 0)
    {
        factor = 23170;
        move16();
        *factor_e = 0;
        move16();
    }
    ELSE IF(sub(L, 4*NORM_MDCT_FACTOR) == 0)
    {
        factor = 16384;
        move16();
        *factor_e = 0;
        move16();
    }
    ELSE
    {

        factor = mult_r(shl(L, 4), 26214 /*128.f / NORM_MDCT_FACTOR*/); /* 4Q11 */
        *factor_e = 4;
        move16();

        factor = ISqrt16(factor, factor_e);
    }

    return factor;
}

static Word16 TCX_MDCT_Inverse_GetScaleFactor(Word16 L, Word16 *factor_e)
{

    Word16 factor;

    IF(sub(L, NORM_MDCT_FACTOR) == 0)
    {
        factor = 32767;
        move16();
        *factor_e = 0;
        move16();
    }
    ELSE IF(sub(L, 2*NORM_MDCT_FACTOR) == 0)
    {
        factor = 23170;
        move16();
        *factor_e = 1;
        move16();
    }
    ELSE IF(sub(L, 4*NORM_MDCT_FACTOR) == 0)
    {
        factor = 32767;
        move16();
        *factor_e = 1;
        move16();
    }
    ELSE
    {
        factor = mult_r(shl(L, 4), 26214 /*128.f / NORM_MDCT_FACTOR*/); /* 4Q11 */
        *factor_e = 4;
        move16();

        factor = Sqrt16(factor, factor_e);
    }

    return factor;
}


void TCX_MDCT(const Word16 *x, Word32 *y, Word16* y_e, Word16 l, Word16 m, Word16 r)
{

    Word16 i;
    Word16 factor, neg_factor;
    Word16 factor_e;

    factor = TCX_MDCT_GetScaleFactor(add(add(shr(l, 1), m), shr(r, 1)), &factor_e);
    *y_e = add(*y_e, factor_e);

    neg_factor = negate(factor);


    /* Init */
    FOR(i=0; i<m/2; i++)
    {
        y[m/2 + r/2 + i]           = L_mult(x [l + m/2 - 1 - i], neg_factor);
        move32();
    }
    FOR(i=0; i<l/2; i++)
    {
        y[m/2 + r/2 + m/2 + i]     = L_msu(L_mult(x[i], factor), x [l - 1 - i], factor);
        move32();
    }
    FOR(i=0; i<m/2; i++)
    {
        y[m/2 + r/2 - 1 - i]       = L_mult(x[l + m/2 + i], neg_factor);
        move32();
    }
    FOR(i=0; i<r/2; i++)
    {
        y[m/2 + r/2 - 1 - m/2 - i] = L_mac(L_mult(x[l+m+i], neg_factor),x[l+m+r-1-i], neg_factor);
        move32();
    }

    *y_e = sub(15, *y_e);
    edct_fx(y, y, l/2 + m + r/2, y_e);
    *y_e = sub(15-1, *y_e);


}

void TCX_MDST(const Word16 *x, Word32 *y, Word16* y_e, Word16 l, Word16 m, Word16 r)
{

    Word16 i;
    Word16 factor, neg_factor;
    Word16 factor_e;

    factor = TCX_MDCT_GetScaleFactor(add(add(shr(l, 1), m), shr(r, 1)), &factor_e);
    *y_e = add(*y_e, factor_e);
    neg_factor = negate(factor);


    /* Init */
    FOR(i=0; i<m/2; i++)
    {
        y[m/2 + r/2 + i]           = L_mult(x [l + m/2 - 1 - i], neg_factor);
        move32();
    }
    FOR(i=0; i<l/2; i++)
    {
        y[m/2 + r/2 + m/2 + i]     = L_msu(L_mult(x[i], neg_factor), x [l - 1 - i], factor);
        move32();
    }
    FOR(i=0; i<m/2; i++)
    {
        y[m/2 + r/2 - 1 - i]       = L_mult(x[l + m/2 + i], neg_factor);
        move32();
    }
    FOR(i=0; i<r/2; i++)
    {
        y[m/2 + r/2 - 1 - m/2 - i] = L_mac(L_mult(x[l+m+i], neg_factor),x[l+m+r-1-i], factor);
        move32();
    }

    *y_e = sub(15, *y_e);
    edst_fx(y, y, l/2 + m + r/2, y_e);
    *y_e = sub(15-1, *y_e);

}

void TCX_MDCT_Inverse(Word32 *x, Word16 x_e, Word16 *y, Word16 l, Word16 m, Word16 r)
{

    Word16 i, fac, negfac, s;
    Word16 L2 = l, R2 = r;
    Word32 tmp_buf[N_MAX+L_MDCT_OVLP_MAX/2];
    Word16 fac_e;

    L2 = shr(l, 1);
    R2 = shr(r, 1);

    x_e = sub(15, x_e);
    edct_fx(x, tmp_buf + L2, l/2 + m + r/2, &x_e);
    x_e = sub(15, x_e);

    fac = TCX_MDCT_Inverse_GetScaleFactor(add(add(shr(l, 1), m), shr(r, 1)), &fac_e);
    x_e = add(x_e, fac_e);

    negfac = negate(fac);

    s = x_e;
    move16();

    FOR (i = 0; i < R2; i++)
    {
        y[l + m + R2 + i] = round_fx(L_shl(Mpy_32_16_1(tmp_buf[L2 + i], negfac), s));   /* fold out right end of DCT */
    }

    FOR (i=0; i<L2; i++)
    {
        y[i] = round_fx(L_shl(Mpy_32_16_1(tmp_buf[L2 + m + R2 + i], fac), s));  /* negate, fold out left end of DCT */
    }

    FOR (i = 0; i < shr(add(L2,add(m,R2)),1); i++)
    {
        Word16 f;
        f =  round_fx(L_shl(Mpy_32_16_1(tmp_buf[L2 + i], negfac), s));

        y[L2 + i] = round_fx(L_shl(Mpy_32_16_1(tmp_buf[l + m + R2 - 1 - i], negfac), s)); /* time-reverse mid of DCT */

        move16();
        y[l + m + R2 - 1 - i] = f;
    }

}


