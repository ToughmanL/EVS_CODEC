/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/

#include "options.h"
#include "basop_util.h"
#include "prot_fx.h"
#include <assert.h>
#include "stl.h"



#define shift_e (16-1)
#define pos_e   (16-1)

static void lerp_proc(Word16 *f, Word16 *f_out,  Word16 bufferNewSize, Word16 bufferOldSize);


void lerp(Word16 *f, Word16 *f_out,  Word16 bufferNewSize, Word16 bufferOldSize)
{
    Word16 tmp1, tmp2, tmpexp;
    BASOP_Util_Divide_MantExp(bufferNewSize, 0, bufferOldSize, 0, &tmp1, &tmpexp);
    tmp1 = shr(tmp1,3); /*Q12*/
    tmp1 = shl(tmp1,tmpexp);

    BASOP_Util_Divide_MantExp(bufferOldSize, 0, bufferNewSize, 0, &tmp2, &tmpexp);
    tmp2 = shr(tmp2,3); /*Q12*/
    tmp2 = shl(tmp2,tmpexp);
    test();
    test();
    IF(sub(tmp1,16224 /*3,9609375 in Q12*/) > 0)
    {
        Word16 tmpNewSize = shl(bufferOldSize,1);
        WHILE(sub(bufferNewSize, bufferOldSize) > 0)
        {
            BASOP_Util_Divide_MantExp(bufferNewSize, 0, bufferOldSize, 0, &tmp1, &tmpexp);
            tmp1 = shr(tmp1,3); /*Q12*/
            tmp1 = shl(tmp1,tmpexp);
            test();
            IF(sub(tmp1,16224 /*3,9609375 in Q12*/) <= 0)
            {
                tmpNewSize = bufferNewSize;
            }

            lerp_proc(f, f_out, tmpNewSize, bufferOldSize);

            f = f_out;
            bufferOldSize = tmpNewSize;
            tmpNewSize = shl(tmpNewSize,1);
        }
    }
    ELSE IF(sub(tmp2,16224 /*3,9609375 in Q12*/) > 0)
    {
        Word16 tmpNewSize = shr(bufferOldSize,1);
        WHILE(sub(bufferNewSize, bufferOldSize) < 0)
        {
            BASOP_Util_Divide_MantExp(bufferOldSize, 0, bufferNewSize, 0, &tmp2, &tmpexp);
            tmp2 = shr(tmp2,3); /*Q12*/
            tmp2 = shl(tmp2,tmpexp);
            test();
            IF(sub(tmp2,16224 /*3,9609375 in Q12*/) <= 0)
            {
                tmpNewSize = bufferNewSize;
            }

            lerp_proc(f, f_out, tmpNewSize, bufferOldSize);

            f = f_out;
            bufferOldSize = tmpNewSize;
            tmpNewSize = shr(tmpNewSize,1);
        }
    }
    else
    {
        lerp_proc(f, f_out, bufferNewSize, bufferOldSize);
    }
}

void lerp_proc(Word16 *f, Word16 *f_out,  Word16 bufferNewSize, Word16 bufferOldSize)
{

    Word16 i, idx, n;
    Word16 diff;
    Word32 pos, shift;
    Word16 buf[2*L_FRAME_MAX];
    Word16 *ptr;


    ptr = f_out;
    test();
    test();
    test();
    if ( ((f <= f_out) && (f + bufferOldSize >= f_out)) || ((f_out <= f) && (f_out + bufferNewSize >= f)) )
    {
        ptr = buf;
        move16();
    }

    IF( sub(bufferNewSize, bufferOldSize) == 0 )
    {
        Copy(f, f_out, bufferNewSize);
        return;
    }

    shift = L_shl(L_deposit_l(div_s( bufferOldSize, shl(bufferNewSize, 4))), 4-shift_e+16);

    pos = L_sub(L_shr(shift, 1), 32768l/*1.0f Q15*/);

    /* Adjust interpolation shift to avoid accessing beyond end of input buffer. */
    if ( L_sub(shift, 19661l/*0.3f Q16*/) < 0)
    {
        pos = L_sub(pos, 8520l/*0.13f Q16*/);
    }

    assert(pos_e == shift_e);

    /* first point of interpolation */
    IF (pos<0)
    {

        diff = shr(extract_l(pos), 1);
        /*buf[0]=f[0]+pos*(f[1]-f[0]);*/
        move16();
        *ptr++ = add(f[0], msu_r(L_mult(diff, f[1]),diff, f[0]));
    }
    ELSE
    {

        idx=extract_h(pos);

        diff = lshr(extract_l(pos), 1);

        move16();
        *ptr++ = add(f[idx], msu_r(L_mult(diff, f[idx+1]), diff, f[idx]));
    }

    pos = L_add(pos, shift);
    idx = s_max(0, extract_h(pos));

    n = sub(bufferNewSize, 1);
    FOR ( i=1; i<n; i++ )
    {
        diff = lshr(extract_l(pos), 1);
        if (pos < 0)
        {
            diff = sub(16384/*0.5f Q15*/, diff);
        }
        move16();
        *ptr++ = add(f[idx], msu_r(L_mult(diff, f[idx+1]), diff, f[idx]));



        pos = L_add(pos, shift);
        idx = extract_h(pos);
    }

    /* last point */

    if ( L_sub(pos, L_deposit_h(sub(bufferOldSize,1))) > 0 )
    {
        idx = sub(bufferOldSize,2);
    }
    assert(idx <= 2*L_FRAME_MAX);

    /* diff = t - point;*/
    diff = lshr(extract_l(L_shr(L_sub(pos, L_deposit_h(idx)), 1)), 1);

    move16();
    *ptr++ = add(f[idx], shl(msu_r(L_mult(diff, f[idx+1]), diff, f[idx]), 1));

    test();
    test();
    test();
    IF ( ((f <= f_out) && (f + bufferOldSize >= f_out)) || ((f_out <= f) && (f_out + bufferNewSize >= f)) )
    {
        Copy( buf, f_out, bufferNewSize );
    }

}
